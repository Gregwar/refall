// TERRARIUM: a sealed glass jar on a windowsill, a tiny ecosystem inside it, seen in 3D behind the well. The jar
// starts with soil, a pebble and a tuft of moss; every cleared line grows something that stays for the rest of the
// song (moss cushions, ferns uncurling, mushrooms, a pond filling, a sapling, flowers), until by scene completion
// (50 lines at the reference pace, paceLines()) the jar is a lush miniature forest. Outside the window the light
// goes from day through golden hour to dusk and night over the same arc, and at dusk the fireflies come out. A
// little kawaii beetle wanders the soil all the while. The song's calm parts bring mist into the jar and
// condensation on its glass; the drop is a summer shower outside the window, the room dimming and every firefly in
// the jar lighting at once. Clears release seeds that drift down inside the jar, behind the board, and sprout where
// they land.
//
// The world is one ray-marched pass at a reduced resolution (shaders.hpp): the window, the sky and the garden behind
// it, the sill, the jar (a revolved profile: its glass refracts a little, thicker and tinted toward its edges, a
// fresnel reflection of the room, condensation droplets that act as small lenses, trickles running down), and the
// world inside (the soil seen in section through the glass, the slots' plants as signed distance fields, the beetle,
// the sprouts; each ray only evaluates the slots whose bounds it comes near). The pieces are pebbles, one clean
// rounded cell each.
#include "universes/terrarium/terrarium.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/terrarium/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

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

GLuint compile(const char* vs, const char* fs, const char* name) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, shaders::COMMON_GLSL);
    std::string v(vs);
    v.insert(v.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(v.c_str(), f.c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant (sRGB): its world (0 the mossy forest, 1 the desert, 2 the tropics), its colors, its seven pieces.
struct VariantDef {
    const char* name;
    int kind;
    unsigned soil, soil2, gravel, strata1, strata2, moss, moss2, leaf, leaf2, stem, cap, cap2, flower, flower2, bark,
        water, stone, bug, bugSpot, fly, sill, frame, wall, hill, hill2;
    unsigned ink, accent, veil;
    unsigned piece[7];
    float pondDepth, spots;
};
const VariantDef VARIANTS[] = {
    // A mossy forest jar: deep earth, emerald moss and ferns, red toadstools, violets, a ladybird.
    {"MOSS", 0, 0x6a4326, 0x2e1b0e, 0x8d98a6, 0x2a2a2e, 0x46464c, 0x8ee04e, 0x2c7d2c, 0x9ae85c, 0x237a36, 0x5f9a3a,
     0xee2f2a, 0xfff2dc, 0x9a62ff, 0xf6eaff, 0x6e4a2e, 0x2a8ab0, 0x9aa8bc, 0xec2a2e, 0x15111a, 0xd8ff6e, 0x6f957e,
     0xf6f1e6, 0xeadcc4, 0x48b03a, 0x84c8a6,
     0xfffaf0, 0xffd27a, 0x0b1c17,
     {0x62c4b0, 0xe8bf5c, 0xa585d6, 0x8cc65e, 0xdc6e58, 0x6a92d0, 0xeda0a8}, 0.07f, 1.f},
    // A desert jar: layered sand in coral and cream, succulent rosettes, aloes, cacti in flower, a turquoise scarab.
    {"DESERT", 1, 0xe2ae74, 0xc48c56, 0xd8c8b4, 0xf47a66, 0xfff0d6, 0xa8c870, 0x6b8a4a, 0x8ad6b4, 0x2f8a7c, 0x8a9a5a,
     0xff7ab0, 0xffe8c0, 0xff5aa0, 0xffd27a, 0xcbb698, 0x4ab0c8, 0xe6d6c4, 0x22c4cc, 0x0d3a5c, 0xffe48a, 0xb8603c,
     0xfff6ea, 0xf2dabb, 0xdc9a4e, 0xeebc84,
     0xfffaf2, 0xffb27a, 0x22130c,
     {0x5cc8bc, 0xf2d282, 0xb48ad8, 0xaac86a, 0xe27a4c, 0x6688c0, 0xec9cba}, 0.0f, 0.f},
    // A tropical jar: rich dark earth, broad striped leaves, magenta orchids, orange caps, a jewel beetle.
    {"ORCHID", 2, 0x4e2c1a, 0x22130a, 0x7a8a72, 0x2a2a2e, 0x46464c, 0x5ad85a, 0x1d6a3a, 0x48d68c, 0x0e5a3a, 0x4a9a4a,
     0xffa83a, 0xfff0d0, 0xff48bc, 0xfff0fa, 0x5a3a28, 0x14a4a4, 0x66788a, 0x3ad46a, 0xe8c440, 0xa8fff0, 0x8a5636,
     0xeaf2ea, 0xdae2ca, 0x26a45a, 0x5ac4a2,
     0xf6fff8, 0xff8ad4, 0x061d18,
     {0x48bfa8, 0xe8c650, 0x9c70d6, 0x6cc46a, 0xe0607e, 0x4e80d6, 0xf09250}, 0.07f, 0.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The slots: type, x, z, size, rotation, birth (lines on the 50-line scale), param.
// The forest's: the start (a pebble, a tuft of moss), then one new thing every ~3 lines, the last at 45.
const Slot SLOTS_MOSS[] = {
    {TerrariumUniverse::S_PEBBLE, 0.80f, 0.12f, 0.11f, 0.6f, -1.f, 0.f},
    {TerrariumUniverse::S_MOSS, -0.55f, 0.36f, 0.18f, 0.f, -1.f, 0.f},
    {TerrariumUniverse::S_MOSS, -0.72f, -0.10f, 0.23f, 0.f, 1.f, 0.f},
    {TerrariumUniverse::S_FERN, -0.62f, -0.45f, 0.66f, 0.3f, 3.5f, 0.f},
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
    {TerrariumUniverse::S_ROSETTE, -0.55f, 0.36f, 0.14f, 0.f, -1.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, -0.72f, -0.10f, 0.18f, 0.4f, 1.f, 0.f},
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
    {TerrariumUniverse::S_MOSS, -0.55f, 0.36f, 0.18f, 0.f, -1.f, 0.f},
    {TerrariumUniverse::S_MOSS, -0.72f, -0.10f, 0.23f, 0.f, 1.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, -0.62f, -0.45f, 0.35f, 0.3f, 3.5f, 2.f},
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
constexpr float SLOT_SPAN = 4.f;   // lines a slot takes to grow
constexpr float POND_AT = 15.f;    // the pond fills from here, over 4 lines

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

// The light outside the window: day, golden hour, dusk, night (sRGB colors and their intensities).
struct Sky {
    unsigned zen, hor, sun, glow, ambS, ambG, fill;
    float sunI, glowI, ambSI, ambGI, fillI, elev, disc, moon, stars, night;
};
// Neighbours keep close hues, so that no blend between them goes grey.
constexpr int SKY_KEYS = 6;
const Sky SKIES[SKY_KEYS] = {
    // day
    {0x2a7ae6, 0xa6dcff, 0xfff1d8, 0xfff2c0, 0x8ab8f0, 0x9a8060, 0xffe8d0, 2.3f, 0.5f, 0.6f, 0.32f, 0.42f, 0.8f, 1.f, 0.f, 0.f, 0.f},
    // afternoon
    {0x2f80e0, 0xffeab4, 0xfff0c8, 0xffe8a8, 0xa8c0e8, 0x9a7a58, 0xffe4c0, 2.3f, 0.6f, 0.55f, 0.3f, 0.4f, 0.42f, 1.f, 0.f, 0.f, 0.f},
    // golden hour
    {0x4072d0, 0xffb44c, 0xffa848, 0xffa040, 0xe0b090, 0x8a5a40, 0xffc890, 2.2f, 1.1f, 0.45f, 0.26f, 0.36f, 0.12f, 1.f, 0.f, 0.f, 0.05f},
    // sunset
    {0x5248b0, 0xff7450, 0xff7040, 0xff6838, 0xd08aa8, 0x6a3a50, 0xffa080, 1.6f, 1.2f, 0.38f, 0.2f, 0.28f, 0.03f, 1.f, 0.f, 0.05f, 0.2f},
    // dusk
    {0x262a7a, 0xd06a9c, 0xff6a6a, 0xe0608a, 0x8a70d0, 0x4a3060, 0xffa0a0, 0.7f, 0.6f, 0.3f, 0.15f, 0.18f, -0.04f, 0.2f, 0.4f, 0.4f, 0.6f},
    // nightfall: the last of the light a violet band low over the hills
    {0x0b1446, 0x4a3a8e, 0x9ab0ff, 0x7a6ad0, 0x4a5ab0, 0x201a40, 0x6070c0, 0.5f, 0.25f, 0.2f, 0.09f, 0.1f, 0.5f, 0.f, 1.f, 1.f, 1.f},
};
// The time of day from the lines (50-line scale): day until 10, then afternoon (18), golden hour (26), sunset (33),
// dusk (40), night (48).
float todOf(float lines) {
    const float K[SKY_KEYS] = {10.f, 18.f, 26.f, 33.f, 40.f, 48.f};
    if (lines <= K[0]) return 0.f;
    for (int i = 1; i < SKY_KEYS; i++)
        if (lines < K[i]) return (float)(i - 1) + smoothstepf(K[i - 1], K[i], lines);
    return (float)(SKY_KEYS - 1);
}
const char* todName(float tod) {
    if (tod < 0.5f) return "morning";
    if (tod < 1.5f) return "afternoon";
    if (tod < 2.5f) return "golden hour";
    if (tod < 3.5f) return "sunset";
    if (tod < 4.5f) return "dusk";
    return "nightfall";
}

constexpr float CLEAR_T = 0.42f; // a cleared row shrinks away in this long (real time)
constexpr int MAX_FLIES = 32, MAX_SEEDS = 32, MAX_SPROUTS = 40, MAX_MOTES = 400;
constexpr float JAR_IN = 0.84f;   // where things may stand (the floor's radius, less a margin)

// Test hooks: ZEN_TERRARIUM_LINES=N adds N lines, ZEN_GROW_RATE=R adds R lines a second.
int extraLines(double t) {
    static const int n = std::getenv("ZEN_TERRARIUM_LINES") ? std::atoi(std::getenv("ZEN_TERRARIUM_LINES")) : 0;
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

// the camera (world units: the jar's radius)
const vec3 EYE(0.f, 0.95f, 3.9f), TARGET(0.f, 0.43f, 0.f);
constexpr float TAN_H = 0.212f;

} // namespace

const std::vector<Slot>& TerrariumUniverse::slots() const { return SLOT_LISTS[wrapV(variant_)]; }

// =====================================================================================================
// Setup
// =====================================================================================================

bool TerrariumUniverse::init(int width, int height) {
    progWorld_ = compile(ugl::FULLSCREEN_VS, shaders::WORLD_FS, "terrarium world");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "terrarium comp");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "terrarium sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "terrarium veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "terrarium cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "terrarium shape");
    progText_ = compile(shaders::TEXT_VS, shaders::TEXT_FS, "terrarium text");
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

void TerrariumUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] terrarium font init failed\n");
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
        for (int rr = 0; rr < h; rr++) std::memcpy(&atlas[(y + rr) * AW + x], sdf + rr * w, w);
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

void TerrariumUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * (float)width_ / height_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    // the world at a reduced resolution (at most 720 lines)
    const int wh = std::max(180, std::min(720, (int)(height_ * 0.66f)));
    const int ww = std::max(320, (int)std::lround((float)wh * width_ / height_));
    worldT_.create(ww, wh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    updateCamera();
}

void TerrariumUniverse::updateCamera() {
    aspect_ = (float)width_ / height_;
    eye_ = EYE;
    fwd_ = normalize(TARGET - EYE);
    right_ = normalize(cross(fwd_, vec3(0.f, 1.f, 0.f)));
    up_ = cross(right_, fwd_);
    tanH_ = TAN_H;
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
    for (ugl::Target* t : {&worldT_, &sceneT_}) t->destroy();
    for (GLuint p : {progWorld_, progComp_, progSprite_, progVeil_, progCell_, progShape_, progText_, progPost_}) glDeleteProgram(p);
    for (GLuint v : {spriteVao_, cellVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, cellVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&caps_, &ital_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
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
    h.background = lerp(ugl::toLinear(hex(s.hor)), p.leaf2, 0.4f) * (tod > 4.f ? 0.4f : 0.8f);
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
    float h = 0.2f;
    const float ax = x + 0.5f, az = z + 0.45f, bx = x - 0.55f, bz = z + 0.5f, cx = x, cz = z + 0.72f;
    h += 0.055f * std::exp(-(ax * ax + az * az) * 5.f) + 0.045f * std::exp(-(bx * bx + bz * bz) * 6.f) +
         0.035f * std::exp(-(cx * cx + cz * cz) * 4.f);
    h -= 0.02f * z;
    h += 0.006f * std::sin(x * 9.f + 1.f) * std::sin(z * 8.f + 2.f);
    const VariantDef& d = VARIANTS[wrapV(variant_)];
    const float px = x - 0.5f, pz = z - 0.42f;
    const float dp = std::sqrt(px * px + pz * pz) / 0.19f;
    h -= d.pondDepth * smoothstepf(0.f, 1.f, saturate(pond_ * 2.f)) * (1.f - smoothstepf(0.5f, 1.15f, dp));
    return h;
}

bool TerrariumUniverse::blocked(float x, float z, float r) const {
    if (x * x + z * z > JAR_IN * JAR_IN) return true;
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

// =====================================================================================================
// Life
// =====================================================================================================

void TerrariumUniverse::addSeed(const vec3& from) {
    if ((int)seeds_.size() >= MAX_SEEDS) return;
    Seed s;
    s.p = from;
    s.v = vec3(0.f);
    float x = 0.f, z = 0.f;
    if (!pickSpot(x, z, 0.03f)) return;
    s.to = vec3(x, soilHeight(x, z), z);
    s.age = -rng_.range(0.f, 0.4f);
    s.life = rng_.range(4.5f, 6.5f);
    s.seed = rng_.uniform();
    s.landed = false;
    seeds_.push_back(s);
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
            if (pickSpot(x, z, 0.07f)) { b.tx = x; b.tz = z; }
        }
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
        if (rr > 0.7f) { hx -= b.x / rr * (rr - 0.7f) * 8.f; hz -= b.z / rr * (rr - 0.7f) * 8.f; }
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
    b.walk += b.speed * dt * 150.f;
    b.look = approach(b.look, lookWant, 2.5f, dt);
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
        // seeds: from inside the jar, behind the cleared rows, drifting down to the soil
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
    // game over: the pebbles dull from the top down
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

float TerrariumUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void TerrariumUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void TerrariumUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float halo) {
    if (q.empty() || !f.ok || alpha <= 0.004f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount);
    set3f(progText_, "uInk", lerp(ink, bridge_.ink, b));
    set3f(progText_, "uHalo", lerp(pal_.veil * 0.7f, bridge_.color, b));
    set1f(progText_, "uHaloA", halo);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD: M PLUS Rounded capitals, spaced; the song, the variant and the notices in Fraunces' soft italics. The
// universe's line ends with the time of day outside the window.
void TerrariumUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
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
    const float halo = 0.9f;
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
            const float sz = base * 0.95f, tr = sz * 0.28f;
            const float wName = textWidth(caps_, name, sz, tr);
            std::string vt = var;
            for (size_t i = 1; i < vt.size(); i++) vt[i] = (char)std::tolower((unsigned char)vt[i]);
            const float vsz = base * 1.3f;
            const float wVar = vt.empty() ? 0.f : textWidth(ital_, vt, vsz, 0.f);
            const float gap = base * 0.7f;
            const std::string when = todName_;
            const float wW = textWidth(ital_, when, vsz, 0.f);
            const float total = wName + (vt.empty() ? 0.f : gap + wVar) + gap * 1.4f + wW;
            const float x = ugl::hudLeft(h, total, width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            textQuads(caps_, name, x, h.y, sz, tr, q);
            drawText(caps_, q, ink, a, halo);
            float cx = x + wName;
            if (!vt.empty()) {
                q.clear();
                textQuads(ital_, vt, cx + gap, h.y - sz * 0.25f, vsz, 0.f, q);
                drawText(ital_, q, accent, a * 0.92f, halo);
                cx += gap + wVar;
            }
            cx += gap * 1.4f;
            q.clear();
            textQuads(ital_, when, cx, h.y - sz * 0.25f, vsz, 0.f, q);
            drawText(ital_, q, ink, a * 0.7f, halo);
            continue;
        }
        case HUD_SECTION:
            place(caps_, h.text, base * 0.9f, base * 0.45f);
            drawText(caps_, q, ink, saturate(0.2f + h.alpha * 1.9f), halo);
            continue;
        case HUD_TITLE:
            place(ital_, h.text, base * 1.7f, base * 0.01f, -base * 0.35f);
            drawText(ital_, q, ink, saturate(h.alpha * 1.3f), halo);
            continue;
        case HUD_INFO:
            place(caps_, h.text, base * 0.66f, base * 0.12f);
            drawText(caps_, q, ink, saturate(alpha * 1.3f) * 0.8f, halo);
            continue;
        case HUD_LABEL:
            place(caps_, h.text, base * 0.8f, base * 0.4f);
            drawText(caps_, q, lerp(ink, accent, 0.5f), saturate(alpha * 1.2f) * 0.95f, halo);
            continue;
        case HUD_VALUE:
            place(caps_, h.text, base * 1.05f, base * 0.06f);
            drawText(caps_, q, ink, alpha, halo);
            continue;
        case HUD_NOTICE:
            place(ital_, h.text, base * 1.6f, base * 0.01f);
            drawText(ital_, q, accent, alpha, halo);
            continue;
        case HUD_KEY:
            place(caps_, h.text, base * 0.75f, base * 0.08f);
            drawText(caps_, q, ink, alpha, halo);
            continue;
        case HUD_TOAST:
            place(caps_, h.text, base * 1.3f, base * 0.45f, -base * 0.2f);
            drawText(caps_, q, ink, alpha, 0.85f);
            continue;
        case HUD_TOAST_SUB:
            place(ital_, h.text, base * 1.4f, base * 0.01f);
            drawText(ital_, q, accent, alpha * 0.9f, 0.85f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(caps_, h.text, base * 1.15f, base * 0.25f, -base * 0.1f);
                drawText(caps_, q, ink, alpha, 0.85f);
            } else {
                place(caps_, h.text, base * 0.85f, base * 0.14f);
                drawText(caps_, q, h.accent ? accent : ink, alpha, 0.85f);
            }
            continue;
        default:
            place(caps_, h.text, base * 0.85f, base * 0.12f);
            drawText(caps_, q, h.accent ? accent : ink, alpha, halo);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void TerrariumUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    // ZEN_GPUTIME: the GPU time of a frame, printed every 60 frames
    static const bool gpuTime = std::getenv("ZEN_GPUTIME") != nullptr;
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
    if (isDrop && !wasDrop_ && std::getenv("ZEN_GPUTIME")) std::fprintf(stderr, "[terrarium] drop at %.1f s\n", time_);
    wasDrop_ = isDrop;
    // the drop: a shower outside the window (settling in over seconds), every firefly lighting at once
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.2f : 0.25f, dt);
    rain_ = approach(rain_, isDrop ? 1.f : 0.f, isDrop ? 0.7f : 0.18f, dt);
    allLit_ = approach(allLit_, isDrop ? 1.f : 0.f, isDrop ? 1.5f : 0.12f, dt);
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    // the calm: mist in the jar, condensation on its glass (slowly)
    {
        const float fogT = lerpf(lerpf(1.f, 0.35f, wm), 0.12f, wp);
        const float condT = lerpf(lerpf(1.f, 0.5f, wm), 0.28f, wp);
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
            // (test hooks starting at a stage: the seedlings its clears would have left)
            if (lines > 0.f && (extraLines(0.0) > 0 || std::getenv("ZEN_LINES_HERE"))) {
                pond_ = VARIANTS[wrapV(variant_)].pondDepth > 0.f ? smoothstepf(POND_AT, POND_AT + 4.f, lines) : 0.f;
                growth_.assign(sl.size(), 0.f);
                for (size_t i = 0; i < sl.size(); i++)
                    growth_[i] = sl[i].birth < 0.f ? 1.f : smoothstepf(sl[i].birth, sl[i].birth + SLOT_SPAN, lines);
                const int n = std::min(MAX_SPROUTS, (int)(lines * 0.8f));
                for (int k = 0; k < n; k++) {
                    float x, z;
                    if (pickSpot(x, z, 0.03f)) sprouts_.push_back({x, z, rng_.range(3.f, 60.f), rng_.uniform()});
                }
            }
        }
        lines_ = lines;
        linesShown_ = lines;
        // the growth eases toward the lines: a line's step is a breath, a Tetris a few (faster in a short song)
        const float cap = (lines < linesEased_ ? 8.f : 1.1f) * std::clamp(paceScale(), 1.f, 2.5f);
        linesEased_ += std::clamp((lines - linesEased_) * 1.6f * wdt, -cap * wdt, cap * wdt);
        if (lines < linesEased_ - 6.f) {
            // a new game: the seedlings fade away
            for (Sprout& s : sprouts_) s.age = std::min(s.age, 1.f);
        }
        const float L = linesEased_;
        if (growth_.size() != sl.size()) growth_.assign(sl.size(), -1.f);
        for (size_t i = 0; i < sl.size(); i++) {
            const float target = sl[i].birth < 0.f ? 1.f : smoothstepf(sl[i].birth, sl[i].birth + SLOT_SPAN, L);
            if (growth_[i] < 0.f) growth_[i] = target;
            growth_[i] = approach(growth_[i], target, 2.5f, wdt);
        }
        const float pt = VARIANTS[wrapV(variant_)].pondDepth > 0.f ? smoothstepf(POND_AT, POND_AT + 4.f, L) : 0.f;
        pond_ = approach(pond_, pt, 2.f, wdt);
        moss_ = smoothstepf(14.f, 50.f, L);
        tod_ = todOf(L);
        todName_ = todName(tod_);
    }
    // the jar keeps growing slowly after its scene is complete
    const float after = 1.f + 0.12f * smoothstepf(50.f, 100.f, linesEased_);

    // ---- The light outside: blend the four keyframes.
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
        // the shower: a slate sky, the sun hidden, the room dimmer
        const float r = rain_;
        sunC = sunC * (1.f - 0.78f * r);
        glowC = glowC * (1.f - 0.7f * r);
        ambS = lerp(ambS, vec3(0.12f, 0.17f, 0.24f) * (1.f - 0.5f * night), 0.65f * r);
        ambG = ambG * (1.f - 0.4f * r);
        fill = fill * (1.f - 0.55f * r);
        disc *= 1.f - r;
        // a peak: a little brighter, steady
        sunC = sunC * (1.f + 0.06f * wp);
        // slow motion: a cool hush
        sunC = lerp(sunC, sunC * vec3(0.7f, 0.8f, 1.1f), slow_);
    }
    vec3 sunDir;
    {
        const float az = -0.33f;
        const float el = std::max(elev, -0.06f);
        const vec3 sd(-std::sin(az) * std::cos(el), std::sin(el), -std::cos(az) * std::cos(el));
        const vec3 md = normalize(vec3(0.48f, 0.42f, -0.77f)); // (the moon, right)
        const float toMoon = smoothstepf(4.f, 4.9f, tod_);
        sunDir = normalize(lerp(sd, md, toMoon));
        if (sunDir.y < 0.08f) sunDir = normalize(vec3(sunDir.x, 0.08f, sunDir.z));
    }

    // ---- Fireflies: they come out at dusk; the drop lights them all at once.
    const float flyAmt = std::max(smoothstepf(33.f, 45.f, linesEased_), allLit_);
    float flyLight = 0.f;
    {
        const float halfBeat = (float)(0.5 + 0.5 * std::cos(beat * PI));   // every two beats
        for (int i = 0; i < (int)flies_.size(); i++) {
            Fly& fl = flies_[i];
            const float th = (float)i / MAX_FLIES * 0.85f;
            const float onT = std::max(smoothstepf(th, th + 0.12f, smoothstepf(33.f, 45.f, linesEased_)), allLit_) *
                              (overT_ >= 0.f ? 0.f : 1.f);
            fl.on = approach(fl.on, onT, onT > fl.on ? 0.8f : 0.4f, dt);
            const float t = time_ * (0.6f + 0.25f * wm + 0.3f * wp);
            fl.p = fl.home + vec3(0.13f * std::sin(t * fl.freq * 0.5f + fl.phase) + 0.05f * std::sin(t * 0.9f + fl.seed * 9.f),
                                  0.07f * std::sin(t * fl.freq * 0.37f + fl.phase * 1.7f),
                                  0.11f * std::cos(t * fl.freq * 0.43f + fl.phase * 0.6f));
            const float r = std::sqrt(fl.p.x * fl.p.x + fl.p.z * fl.p.z);
            if (r > 0.86f) { fl.p.x *= 0.86f / r; fl.p.z *= 0.86f / r; }
            fl.p.y = std::max(fl.p.y, soilHeight(fl.p.x, fl.p.z) + 0.06f);
            // a soft slow glow, eased (no flashes); in a peak they swell together every two beats
            const float pulse = smoothstepf(-0.6f, 0.9f, std::sin(time_ * fl.freq * 1.1f + fl.phase));
            float g = (0.3f + 0.7f * pulse) * (1.f + 0.3f * wp * halfBeat * smoothstepf(0.2f, 0.6f, pulse) + 0.25f * kickS_);
            g = lerpf(g, std::max(g, 0.9f), allLit_);
            fl.glow = g * fl.on;
            flyLight += fl.glow;
        }
    }
    (void)flyLight;

    // ---- Seeds drifting down and the sprouts they leave.
    for (Seed& s : seeds_) {
        s.age += wdt;
        if (s.age < 0.f || s.landed) continue;
        if (s.age >= s.life) {
            s.landed = true;
            // (one seed in two takes root; the others sink into the moss)
            if ((int)sprouts_.size() < MAX_SPROUTS && s.seed < 0.55f) sprouts_.push_back({s.to.x, s.to.z, 0.f, s.seed});
        }
    }
    seeds_.erase(std::remove_if(seeds_.begin(), seeds_.end(), [](const Seed& s) { return s.landed; }), seeds_.end());
    for (Sprout& s : sprouts_) s.age += (s.age < 1.f && lines_ < linesEased_ - 6.f) ? -wdt : wdt;
    if (lines_ < linesEased_ - 6.f)
        for (Sprout& s : sprouts_) s.age = std::min(s.age, 1.f) - wdt * 0.6f;
    sprouts_.erase(std::remove_if(sprouts_.begin(), sprouts_.end(), [](const Sprout& s) { return s.age < -0.05f; }), sprouts_.end());

    if (!f.paused) updateBeetle(wdt, wm, wp);

    // ---- Motes over the board.
    for (Mote& m : motes_) {
        m.age += dt;
        m.p = vec2{m.p.x + m.v.x * dt, m.p.y + m.v.y * dt};
        m.v = vec2{m.v.x * std::exp(-2.5f * dt), m.v.y * std::exp(-2.5f * dt)};
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.age >= m.life; }), motes_.end());
    for (auto& fd : fading_) fd.t += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > CLEAR_T; }), fading_.end());
    if (!f.bare) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);
    sprites_.clear();
    if (!f.bare) {
        for (const Mote& m : motes_) {
            const float t = m.age / m.life;
            const float a = smoothstepf(0.f, 0.1f, t) * (1.f - smoothstepf(0.5f, 1.f, t)) * m.alpha;
            sprite(m.p, m.size * (1.f + t), m.col, a, 0);
        }
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const bool on = game.bonusReady() || game.bonusActive();
        sprite(V(gc.x, gc.y), layout::GAUGE_R * 1.25f, on ? pal_.fly : pal_.accent, 1.f, 1, fill, on ? 1.f : 0.f);
    }
    cells_.clear();
    if (!f.bare) buildBoard(game);

    // =========================================================== the world (reduced resolution)
    {
        worldT_.bind();
        glDisable(GL_BLEND);
        const GLuint P = progWorld_;
        glUseProgram(P);
        set3f(P, "uEye", eye_);
        set3f(P, "uFwd", fwd_);
        set3f(P, "uRight", right_);
        set3f(P, "uUp", up_);
        set1f(P, "uTanH", tanH_);
        set1f(P, "uAspect", aspect_);
        set1f(P, "uTime", std::fmod(time_, 3000.f));
        set1f(P, "uPx", 2.f * tanH_ / worldT_.h);
        const VariantDef& vd = VARIANTS[wrapV(variant_)];
        set1i(P, "uKind", vd.kind);
        // the slots
        {
            float A[24 * 4] = {}, B[24 * 4] = {}, C[24 * 4] = {};
            const int n = std::min(24, (int)sl.size());
            for (int i = 0; i < n; i++) {
                const Slot& s = sl[i];
                A[i * 4 + 0] = s.x;
                A[i * 4 + 1] = soilHeight(s.x, s.z);
                A[i * 4 + 2] = s.z;
                A[i * 4 + 3] = s.size * after;
                B[i * 4 + 0] = (float)s.type;
                B[i * 4 + 1] = std::max(0.f, growth_[i]);
                B[i * 4 + 2] = (float)i * 1.37f + 0.5f;
                B[i * 4 + 3] = s.rot;
                C[i * 4 + 0] = s.param;
            }
            set1i(P, "uSlots", n);
            glUniform4fv(ugl::U(P, "uSA"), 24, A);
            glUniform4fv(ugl::U(P, "uSB"), 24, B);
            glUniform4fv(ugl::U(P, "uSC"), 24, C);
        }
        {
            float S[MAX_SPROUTS * 4] = {};
            const int n = std::min(MAX_SPROUTS, (int)sprouts_.size());
            for (int i = 0; i < n; i++) {
                const Sprout& s = sprouts_[i];
                S[i * 4 + 0] = s.x;
                S[i * 4 + 1] = soilHeight(s.x, s.z) - 0.004f;
                S[i * 4 + 2] = s.z;
                S[i * 4 + 3] = saturate(s.age / 3.f) * (0.85f + 0.15f * smoothstepf(5.f, 30.f, s.age));
            }
            set1i(P, "uSprouts", n);
            glUniform4fv(ugl::U(P, "uSp"), MAX_SPROUTS, S);
        }
        {
            float F[MAX_FLIES * 4] = {};
            int n = 0;
            for (const Fly& fl : flies_) {
                if (fl.glow <= 0.01f) continue;
                F[n * 4 + 0] = fl.p.x; F[n * 4 + 1] = fl.p.y; F[n * 4 + 2] = fl.p.z; F[n * 4 + 3] = fl.glow;
                n++;
            }
            set1i(P, "uFlies", n);
            glUniform4fv(ugl::U(P, "uFly"), MAX_FLIES, F);
        }
        {
            float S[MAX_SEEDS * 4] = {};
            int n = 0;
            for (const Seed& s : seeds_) {
                if (s.age < 0.f) continue;
                const float t = saturate(s.age / s.life);
                // out from behind the well first, then floating down to the soil
                const float eh = 1.f - std::pow(1.f - t, 2.6f);
                const float ev = t * t * (3.f - 2.f * t);
                vec3 p = lerp(s.p, s.to + vec3(0.f, 0.012f, 0.f), eh);
                p.y = lerpf(s.p.y, s.to.y + 0.012f, ev);
                const float sw = std::sin(s.age * 1.9f + s.seed * 20.f) * 0.05f * (1.f - t);
                p.x += sw;
                p.z += std::cos(s.age * 1.4f + s.seed * 13.f) * 0.03f * (1.f - t);
                const float a = smoothstepf(0.f, 0.4f, s.age) * (1.f - smoothstepf(0.85f, 1.f, t) * 0.6f);
                S[n * 4 + 0] = p.x; S[n * 4 + 1] = p.y; S[n * 4 + 2] = p.z; S[n * 4 + 3] = a;
                n++;
            }
            set1i(P, "uSeeds", n);
            glUniform4fv(ugl::U(P, "uSeedP"), MAX_SEEDS, S);
        }
        set4f(P, "uBug", bug_.x, soilHeight(bug_.x, bug_.z) - 0.002f, bug_.z, bug_.head);
        set4f(P, "uBug2", bug_.walk, saturate(bug_.speed / 0.03f), bug_.look, bug_.blink > 0.f ? std::sin(bug_.blink * PI) : 0.f);
        set4f(P, "uPondP", 0.5f, 0.42f, 0.19f, vd.pondDepth * smoothstepf(0.f, 1.f, saturate(pond_ * 2.f)));
        set1f(P, "uPondY", 0.2f - 0.0084f - vd.pondDepth * (1.f - 0.85f * pond_) * 0.6f - 0.004f);
        set1f(P, "uPond", pond_);
        set1f(P, "uMossCover", moss_);
        set1f(P, "uSway", 0.35f + 0.35f * wm + 0.3f * wp);
        set1f(P, "uSpots", vd.spots);
        set3f(P, "uSunDir", sunDir);
        set3f(P, "uSunCol", sunC);
        set3f(P, "uSkyZ", skyZ);
        set3f(P, "uSkyH", skyH);
        set3f(P, "uAmbS", ambS);
        set3f(P, "uAmbG", ambG);
        set3f(P, "uFill", fill);
        set3f(P, "uGlowSun", glowC);
        set1f(P, "uSunDisc", disc);
        // the sun as the window shows it (stretched elevations): low over the hills on the left
        set2f(P, "uSunSky", 0.3f, std::clamp(elev, -0.1f, 0.9f) * 2.0f);
        set1f(P, "uMoon", moon);
        set1f(P, "uStars", stars * (1.f - rain_));
        set1f(P, "uRain", rain_);
        set1f(P, "uFog", fog_ * (0.7f + 0.3f * (1.f - wp)));
        set1f(P, "uCond", cond_);
        set1f(P, "uNight", night);
        set1f(P, "uFlyAmt", flyAmt);
        set3f(P, "uSoil", pal_.soil); set3f(P, "uSoil2", pal_.soil2); set3f(P, "uGravel", pal_.gravel);
        set3f(P, "uStr1", pal_.strata1); set3f(P, "uStr2", pal_.strata2); set3f(P, "uMoss", pal_.moss);
        set3f(P, "uMoss2", pal_.moss2); set3f(P, "uLeaf", pal_.leaf); set3f(P, "uLeaf2", pal_.leaf2);
        set3f(P, "uStem", pal_.stem); set3f(P, "uCap", pal_.cap); set3f(P, "uCap2", pal_.cap2);
        set3f(P, "uFlower", pal_.flower); set3f(P, "uFlower2", pal_.flower2); set3f(P, "uBark", pal_.bark);
        set3f(P, "uWater", pal_.water); set3f(P, "uStone", pal_.stone); set3f(P, "uBugC", pal_.bug);
        set3f(P, "uBugSpot", pal_.bugSpot); set3f(P, "uFlyC", pal_.fly * 1.4f); set3f(P, "uSill", pal_.sill);
        set3f(P, "uFrame", pal_.frame); set3f(P, "uWall", pal_.wall); set3f(P, "uHill", pal_.hill);
        set3f(P, "uHill2", pal_.hill2);
        set3f(P, "uBridgeCol", bridge_.color);
        set1f(P, "uWorldA", world);
        set1f(P, "uSlow", slow_);
        ugl::drawFullscreen();
    }

    // =========================================================== the scene (full resolution)
    sceneT_.bind();
    glUseProgram(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progComp_, "uWorld", 0);
    set2f(progComp_, "uTexel", 1.f / worldT_.w, 1.f / worldT_.h);
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
        set3f(progCell_, "uLightC", vec3(1.f, 0.97f, 0.9f));
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    if (!sprites_.empty()) {
        glUseProgram(progSprite_);
        setMat(progSprite_, "uVP", vp_);
        set1f(progSprite_, "uPxPerUnit", pxPerUnit_);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    drawHud(f.hud, f.panels);
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
        "TERRARIUM", "a sealed glass jar on a windowsill; every cleared line grows its little world, moss, ferns, mushrooms, a pond, until it is a lush forest at dusk full of fireflies",
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
