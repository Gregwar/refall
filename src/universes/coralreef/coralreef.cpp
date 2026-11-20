// CORAL REEF: a coral reef growing with the cleared lines, in one continuous camera pull-back that never leaves
// the water, under a sunny toon light (caustics, shafts of light, a rippling surface). It opens under a macro lens on
// a single polyp a centimetre tall on a pebble (the sand's grains big, a shallow depth of field); the polyp buds
// into a coral head with its goby, boulders rise into a bommie with its colonies and little fish, the camera pulls
// back to the patch reef (its schools, its clownfish), colonies spread over the floor into a barrier reef as the
// camera draws back; then the reef grows up, not out: the ground around it sinks away while it stacks up into a
// coral tower, the place it began its sunlit summit (glowing softly, a great school circling above it), its flanks
// built of colonies on colonies plunging in terraces and ledges into the blue. The camera is keyed on the lines
// (CAMKEYS); its depth range follows its scale and the world is drawn relative to the eye. The stages: 0 the polyp,
// 5 the coral head, 9 the bommie, 17 the// reef, 31 a mound, 36 a coral stack, 42 a coral tower, 47 a coral mountain, 50 completion (paceLines()). Every cleared
// row bursts into bubbles behind the well and its glowing seeds settle where the next corals grow.
//
// The music: calm is still, slow water; mid stirs the fish and the surge; a peak brightens the light shafts, the
// schools swim fast and tight and the kicks pulse the caustics (a little brighter, the water surging on). The drop
// brings a manta ray gliding past behind the well, its wings beating slowly. A quad: the schools gather and
// swirl together behind the well for a while. Slow motion turns the light deep blue. Game over: the light sinks
// to the deep, the reef recedes for the next game. Around a song the camera dives down onto its view as the light
// comes down from the surface, and after it drifts back as the light sinks to the deep.
//
// The pieces: each cell a chunk of living coral filling it, its edges soft, rounded and irregular, a species per
// piece type (staghorn, star coral, porous coral, plate coral, tube coral, sponge, brain coral; each in two forms, a
// colony's one or the other), its relief lit softly from above with a faint ripple of light. A piece is one colony
// (its blocks grow into each other, a dark seam between two colonies), each block its own variation, both kept
// through the falls and the clears (see CELL_FS). The ghost is a thin crust of coral along the piece's outline. A
// cleared row shrinks away in place; its bubbles and seeds travel in the world behind.
//
// Cost: the world at a reduced resolution (at most 720 lines) with depth (the water, ~115k triangles of seabed, the
// corals instanced per form, the fish, the billboards); the composite at full resolution integrates the light
// shafts in ten steps a pixel. The blocks' relief is sampled three times a pixel (a full board: about 0.05 ms of an
// RTX 4090 at 1080p, 0.16 ms at 4K).
#include "universes/coralreef/coralreef.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/coralreef/shaders.hpp"
#include "universes/registry.hpp"

namespace {

// Fraunces Soft Italic (the song, the variant, the notices) and Nunito SemiBold (labels, numbers), SIL Open Font
// License, see OFL-Fraunces.txt and OFL-Nunito.txt.
const unsigned char FONT_ITAL[] = {
#include "universes/coralreef/Fraunces-SoftItalic.ttf.inc"
};
const unsigned char FONT_SANS[] = {
#include "universes/coralreef/Nunito-SemiBold.ttf.inc"
};

namespace shaders = coralreef_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compile(const char* vs, const char* fs, const char* name, bool world) {
    const std::string pre = std::string(shaders::COMMON_GLSL) + (world ? shaders::REEF_GLSL : "");
    std::string f(fs);
    f.insert(f.find('\n') + 1, pre);
    std::string v(vs);
    v.insert(v.find('\n') + 1, pre);
    return ugl::program(v.c_str(), f.c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned c) { return ugl::toLinear(hex(c)); }
float sqf(float x) { return x * x; }

// A variant (sRGB): the water (near, deep, the bright haze up toward the surface, the surface seen from below),
// the light (sun, shade, rim, caustics, shafts); the sand (two tones), the rock (two tones) and its crusts (pink
// coralline, green turf); the reef's eight colors (0 pink, 1 violet, 2 yellow, 3 orange, 4 mint, 5 magenta,
// 6 lime, 7 blue: each coral form picks its pairs among them); the HUD's ink and accent, the veil and the well's
// rails; the seven pieces; the sun's place (azimuth, elevation), the water's clarity (fog distance, meters), the
// shafts, the caustics, the corals' glow (fluorescence), night, saturation.
struct VariantDef {
    const char* name;
    unsigned water, deep, haze, surface, sun, shade, rim, caustic, ray;
    unsigned sand, sand2, rock, rock2, crust, crust2;
    unsigned reef[8];
    unsigned ink, accent, veil, rail;
    unsigned piece[7];
    float sunAz, sunEl, fog, rays, caustics, glow, night, sat;
};
const VariantDef VARIANTS[] = {
    // A lagoon at midday: turquoise water over white sand, a high sun, the reef in every color.
    {"LAGOON", 0x0fb8cc, 0x0646a0, 0x6ce4ee, 0xc4fcff, 0xfff6dc, 0x6a8ee0, 0xa8f6ff, 0xfffbe8, 0xeafff8,
     0xf2e0bc, 0xe0c294, 0xa47c6c, 0x6e5470, 0xf07aa0, 0x66c45a,
     {0xff5f8a, 0xa45cff, 0xffd02a, 0xff8a2a, 0x2fe0a8, 0xff3fd0, 0x9cf03a, 0x3a9cff},
     0xffffff, 0xffd84a, 0x022a56, 0x86ecf2,
     {0x7cc4b4, 0xf2c050, 0xf0a890, 0x9aa04c, 0xd0605c, 0x6a9cd0, 0xe87c48}, 0.55f, 1.18f, 27.f, 1.f, 1.f, 0.08f, 0.f, 1.1f},
    // A reef at golden hour: green-gold water, the sun low on one side, long warm shafts, warm corals.
    {"GOLDEN HOUR", 0x1aa2b4, 0x0a3a7c, 0xffc890, 0xffe2b0, 0xffcf88, 0x7a76c8, 0xffcc88, 0xffe2a4, 0xffcf84,
     0xf0dcc0, 0xdcb894, 0x8c6e6c, 0x6a5868, 0xff6a7a, 0xa0c048,
     {0xff5a5a, 0xa060ff, 0xffd23a, 0xff8a2a, 0x3ad0a0, 0xe048a8, 0xb0e048, 0x5a8cff},
     0xfff6e8, 0xffb84a, 0x0a1e46, 0xffcf84,
     {0x8cb8a8, 0xffcc44, 0xe8ac88, 0x989c4e, 0xd88474, 0x5a80a0, 0xe87c4c}, -1.15f, 0.62f, 23.f, 1.35f, 0.85f, 0.06f, 0.f, 1.12f},
    // The outer reef: deep cobalt water, crystal clear, a white sun high above.
    {"OUTER REEF", 0x1478d8, 0x062a80, 0x62bcff, 0xa4dcff, 0xf4fbff, 0x5a74d0, 0x90d4ff, 0xf0f8ff, 0xd8f0ff,
     0xd4deec, 0xb0bed6, 0x7c6c8a, 0x5a4c74, 0xd060c8, 0x48b890,
     {0xff6a7a, 0x9a5aff, 0xffd23a, 0xff7a30, 0x3ae0d0, 0xff4aa0, 0x8cf060, 0x4a8cff},
     0xffffff, 0x7af0ff, 0x02164a, 0x90d4ff,
     {0x7ab8c0, 0xe8c060, 0xe8a49a, 0x8c9a58, 0xc05a6a, 0x6a90d8, 0xe0784e}, 0.3f, 1.3f, 31.f, 1.1f, 1.f, 0.08f, 0.f, 1.1f},
    // A moonlit night dive: indigo water, moonlight shafts, the corals glowing in fluorescent colors.
    {"MOONLIT", 0x0c3a7a, 0x03102e, 0x2a5ab8, 0x3a6ad8, 0xb8ccff, 0x2a3a88, 0x6aa0ff, 0xa8c8ff, 0x8ab0ff,
     0x8a9ac8, 0x6a78a8, 0x3a3c5a, 0x2a2c48, 0xc04aa0, 0x2a9a7a,
     {0xff5a8a, 0xb060ff, 0xffe040, 0xffa030, 0x30ffe0, 0xff40d0, 0xa0ff40, 0x6a7aff},
     0xeef4ff, 0x6affe8, 0x01061a, 0x4a80ff,
     {0x60e0c8, 0xf0d860, 0xff9cb0, 0xa8d860, 0xff6a8a, 0x7a9cff, 0xffa060}, 0.9f, 1.05f, 21.f, 0.75f, 0.7f, 0.9f, 1.f, 1.1f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));
int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }

using Palette = CoralReefUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[wrapV(v)];
    Palette p;
    p.water = lin(d.water); p.deep = lin(d.deep); p.haze = lin(d.haze); p.surface = lin(d.surface);
    p.sun = lin(d.sun); p.shade = lin(d.shade); p.rim = lin(d.rim); p.caustic = lin(d.caustic); p.ray = lin(d.ray);
    p.sand = lin(d.sand); p.sand2 = lin(d.sand2); p.rock = lin(d.rock); p.rock2 = lin(d.rock2);
    p.crust = lin(d.crust); p.crust2 = lin(d.crust2);
    for (int i = 0; i < 8; i++) p.reef[i] = lin(d.reef[i]);
    p.ink = lin(d.ink); p.accent = lin(d.accent); p.veil = lin(d.veil); p.rail = lin(d.rail);
    for (int i = 0; i < 7; i++) p.piece[i] = lin(d.piece[i]);
    p.sunAz = d.sunAz; p.sunEl = d.sunEl; p.fog = d.fog; p.rays = d.rays; p.caustics = d.caustics;
    p.glow = d.glow; p.night = d.night; p.sat = d.sat;
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

constexpr float CLEAR_T = 0.36f;     // a cleared row shrinks away in this long (real time)
constexpr float GROW_T = 3.2f;       // a coral grows in this long once its seed has settled
constexpr int N_SLOTS = 176;         // corals at scene completion (and a few more, slowly, after it)
constexpr int MAX_BUBBLES = 1400;
constexpr int MAX_EGGS = 3200;      // the drop's coral spawn
constexpr float CAM_FOV = 56.f;      // degrees, vertical
const vec3 EYE0(0.f, 3.1f, 15.5f), TARGET0(0.f, 1.3f, 0.f);

// When each coral form joins the reef (lines on the 50-line scale) and how big it is (meters for the template).
struct KindDef { float unlock, weight, sizeLo, sizeHi, sway, rockPref; };
const KindDef KINDS[reef::K_COUNT] = {
    {0.f, 0.55f, 1.0f, 1.6f, 0.f, 1.6f},   // brain
    {4.f, 1.3f, 1.3f, 2.0f, 0.03f, 1.6f},  // staghorn
    {24.f, 0.7f, 1.0f, 1.5f, 0.f, 1.8f},   // table
    {9.f, 0.9f, 1.6f, 2.5f, 0.22f, 1.5f},  // sea fan
    {14.f, 0.8f, 1.2f, 1.8f, 0.f, 1.3f},   // tube sponges
    {1.5f, 0.6f, 1.3f, 1.9f, 0.5f, 0.8f},  // anemone
    {19.f, 0.7f, 1.3f, 1.9f, 0.3f, 1.2f},  // sea whips
};
// Color pairs (indices into the reef's eight colors) per form.
const int PAIRS[reef::K_COUNT][4][2] = {
    {{2, 6}, {4, 6}, {1, 7}, {3, 2}},
    {{1, 7}, {7, 4}, {0, 2}, {3, 2}},
    {{4, 6}, {2, 6}, {7, 4}, {0, 3}},
    {{5, 0}, {0, 3}, {1, 5}, {3, 2}},
    {{1, 5}, {3, 2}, {2, 3}, {5, 1}},
    {{0, 6}, {3, 2}, {5, 4}, {1, 4}},
    {{2, 3}, {0, 5}, {3, 0}, {7, 1}},
};

// The schools: species, the line they come at, how many fish at scene completion, size, their stretch of reef.
struct SchoolDef { int species; float appear; int count; float size; float x0, x1; };
const SchoolDef SCHOOLS[] = {
    {reef::F_CHROMIS, 17.f, 20, 0.7f, -13.f, -6.f},
    {reef::F_ANTHIAS, 19.f, 26, 0.7f, 6.f, 13.f},
    {reef::F_CHROMIS, 22.f, 24, 0.7f, 5.f, 14.f},
    {reef::F_YELLOW, 24.f, 12, 1.0f, -13.f, -5.f},
    {reef::F_BLUE, 26.f, 10, 1.0f, 5.f, 13.f},
    {reef::F_ANTHIAS, 28.f, 26, 0.7f, -14.f, -6.f},
    {reef::F_IDOL, 30.f, 4, 1.05f, -11.f, 11.f},
    {reef::F_PARROT, 32.f, 4, 1.4f, -12.f, 12.f},
    {reef::F_CHROMIS, 33.f, 36, 0.66f, -10.f, 10.f},
};
constexpr float SPECIES_SPEED[reef::F_COUNT] = {1.5f, 1.45f, 1.1f, 1.2f, 0.9f, 1.0f, 0.6f};

// Test hooks: REFALL_CORALREEF_LINES=N adds N lines to the reef, REFALL_GROW_RATE=R adds R lines a second.
int extraLines(double t) {
    static const int n = std::getenv("REFALL_CORALREEF_LINES") ? std::atoi(std::getenv("REFALL_CORALREEF_LINES")) : 0;
    static const double rate = std::getenv("REFALL_GROW_RATE") ? std::atof(std::getenv("REFALL_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

const char* stageName(float L) {
    if (L < 4.5f) return "a polyp";
    if (L < 9.f) return "a coral head";
    if (L < 17.f) return "a bommie";
    if (L < 31.f) return "the reef";
    if (L < 36.f) return "a mound";
    if (L < 42.f) return "a coral stack";
    if (L < 47.f) return "a coral tower";
    return "a coral mountain";
}

// The growth's camera, keyed on the lines (the 50-line scale): the distance to what it frames (meters, eased in
// log space), its pitch (degrees), where that lies on the screen (NDC, in the open band right of the HUD while it
// is small), and what it frames (0: the polyp, 1: the reef). A macro lens on a polyp a centimetre tall, a coral
// head, a bommie, the reef (the reef stage's framing at 30), then the same reef from ever further away.
struct CamKey { float L, D, pitch, sx, sy, anchor; };
const CamKey CAMKEYS[] = {
    {0.f, 0.045f, -12.f, 0.7f, -0.1f, 0.f},  {3.f, 0.07f, -13.f, 0.7f, -0.1f, 0.f},
    {5.f, 0.16f, -14.f, 0.7f, -0.12f, 0.f},  {8.f, 0.38f, -16.f, 0.7f, -0.1f, 0.f},
    {13.f, 1.3f, -17.f, 0.7f, -0.06f, 0.f},
    {18.f, 2.6f, -14.f, 0.66f, -0.04f, 0.04f}, {24.f, 7.5f, -9.f, 0.42f, 0.f, 0.5f},
    {30.f, 15.6f, -6.6f, 0.f, 0.f, 1.f},
    // then the reef stacking up into a tower under it: the camera keeps the place it began in view right of the well,
    // drawing back and down round the tower (anchor 2: the summit; 3: the tower's middle), looking up its flanks,
    // then out at the whole tower against the blue
    {33.f, 26.f, -9.f, 0.35f, -0.02f, 1.6f},   {36.f, 48.f, -3.f, 0.5f, 0.f, 2.f},
    {39.f, 90.f, 7.f, 0.52f, 0.04f, 2.3f},     {42.f, 165.f, 8.f, 0.5f, 0.03f, 2.6f},
    {45.f, 280.f, 5.f, 0.48f, 0.f, 2.85f},     {48.f, 450.f, 2.f, 0.45f, 0.f, 3.f},
    {50.f, 560.f, 1.f, 0.44f, 0.f, 3.f},       {56.f, 700.f, 0.f, 0.42f, 0.f, 3.f},
};
constexpr int CAMKEY_COUNT = (int)(sizeof(CAMKEYS) / sizeof(CAMKEYS[0]));
CamKey camAt(float L) {
    L = std::clamp(L, CAMKEYS[0].L, CAMKEYS[CAMKEY_COUNT - 1].L);
    int i = 0;
    while (i + 2 < CAMKEY_COUNT && CAMKEYS[i + 1].L <= L) i++;
    const CamKey& a = CAMKEYS[i];
    const CamKey& b = CAMKEYS[i + 1];
    const float t = saturate((L - a.L) / (b.L - a.L));
    CamKey k;
    k.L = L;
    k.D = std::exp(lerpf(std::log(a.D), std::log(b.D), t));
    k.pitch = lerpf(a.pitch, b.pitch, t);
    k.sx = lerpf(a.sx, b.sx, t);
    k.sy = lerpf(a.sy, b.sy, t);
    k.anchor = lerpf(a.anchor, b.anchor, t);
    return k;
}
// The growth's large stages: the reef carpet spreading in every direction (meters from where it began).
// The tower's height with the lines (meters, its floor at -H): it stacks up from the reef stage on.
float towerAt(float L) { return reef::TOWER_MAX * std::pow(smoothstepf(30.5f, 50.f, L), 1.25f) + 6.f * smoothstepf(50.f, 56.f, L); }
// The lines at which the tower reaches a depth (the inverse of towerAt, by bisection).
float towerLineFor(float depth) {
    float lo = 30.5f, hi = 50.f;
    if (depth >= towerAt(hi)) return hi;
    for (int i = 0; i < 30; i++) { const float m = 0.5f * (lo + hi); (towerAt(m) < depth ? lo : hi) = m; }
    return hi;
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

void CoralReefUniverse::upload(MeshGL& m, const reef::Mesh& mesh, int instFloats) {
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ibo);
    glGenBuffers(1, &m.inst);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.v.size() * sizeof(float), mesh.v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 10, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 10, (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 10, (void*)(sizeof(float) * 6));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.idx.size() * sizeof(uint32_t), mesh.idx.data(), GL_STATIC_DRAW);
    m.count = (GLsizei)mesh.idx.size();
    glBindBuffer(GL_ARRAY_BUFFER, m.inst);
    for (int i = 0; i < instFloats / 4; i++) {
        glEnableVertexAttribArray(3 + i);
        glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(float) * instFloats, (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(3 + i, 1);
    }
    glBindVertexArray(0);
}

bool CoralReefUniverse::init(int width, int height) {
    progWater_ = compile(ugl::FULLSCREEN_VS, shaders::WATER_FS, "coralreef water", true);
    progGround_ = compile(shaders::GROUND_VS, shaders::GROUND_FS, "coralreef ground", true);
    progCoral_ = compile(shaders::CORAL_VS, shaders::CORAL_FS, "coralreef coral", true);
    progFish_ = compile(shaders::FISH_VS, shaders::FISH_FS, "coralreef fish", true);
    progGlow_ = compile(shaders::GLOW_VS, shaders::GLOW_FS, "coralreef glow", true);
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "coralreef comp", true);
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "coralreef veil", false);
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "coralreef cell", false);
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "coralreef sprite", false);
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "coralreef shape", false);
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "coralreef post", false);

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
    instanced(glowVao_, glowVbo_, 3, sizeof(float) * 12);
    instanced(spriteVao_, spriteVbo_, 2, sizeof(SpriteInst));
    instanced(cellVao_, cellVbo_, 3, sizeof(CellInst));
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));

    // the seabed: a log-polar mesh around the view (rings log-spaced from a fraction of the camera's scale out to
    // the horizon), placed each frame where the camera looks: fine where it looks, the whole planet beyond
    {
        const int NR = 700, NA = 288;
        std::vector<float> g;
        g.reserve((NR + 1) * (NA + 1) * 2);
        for (int j = 0; j <= NR; j++)
            for (int i = 0; i <= NA; i++) {
                g.push_back((float)j / NR);
                g.push_back(TAU * i / NA);
            }
        std::vector<uint32_t> idx;
        idx.reserve(NR * NA * 6);
        for (int j = 0; j < NR; j++)
            for (int i = 0; i < NA; i++) {
                const uint32_t a = j * (NA + 1) + i, b = a + 1, c = a + (NA + 1), d = c + 1;
                idx.insert(idx.end(), {a, c, b, b, c, d});
            }
        glGenVertexArrays(1, &groundVao_);
        glGenBuffers(1, &groundVbo_);
        glGenBuffers(1, &groundIbo_);
        glBindVertexArray(groundVao_);
        glBindBuffer(GL_ARRAY_BUFFER, groundVbo_);
        glBufferData(GL_ARRAY_BUFFER, g.size() * sizeof(float), g.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundIbo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);
        groundCount_ = (GLsizei)idx.size();
    }
    for (int k = 0; k < reef::K_COUNT; k++)
        for (int t = 0; t < reef::TEMPLATES; t++) upload(coral_[k][t], reef::buildCoral(k, t), 16);
    for (int t = 0; t < reef::TEMPLATES; t++) upload(rockM_[t], reef::buildRock(t), 16);
    for (int s = 0; s < reef::F_COUNT; s++) upload(fishM_[s], reef::buildFish(s), 12);
    upload(manta_, reef::buildManta(), 12);
    glBindVertexArray(0);

    pal_ = basePal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void CoralReefUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    aspect_ = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    lowH_ = std::max(180, std::min(720, (int)(height_ * 0.7f)));
    const int wh = std::max(180, std::min(1440, height_));
    const int ww = std::max(320, (int)std::lround((float)wh * aspect_));
    msT_.create(ww, wh, 4);
    worldT_.create(ww, wh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    refVP_ = mat4::perspective(CAM_FOV * PI / 180.f, aspect_, 0.1f, 300.f) *
             mat4::lookAt(vec3(0.f), normalize(TARGET0 - EYE0), vec3(0, 1, 0));
    updateCamera();
    reefDirty_ = true; // (the slots favor what the reef's framing sees)
}

CoralReefUniverse::~CoralReefUniverse() {
    for (ugl::Target* t : {&worldT_, &sceneT_}) t->destroy();
    msT_.destroy();
    for (GLuint p : {progWater_, progGround_, progCoral_, progFish_, progGlow_, progComp_, progVeil_, progCell_, progSprite_,
                     progShape_, progPost_})
        glDeleteProgram(p);
    auto freeMesh = [](MeshGL& m) {
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
        for (GLuint b : {m.vbo, m.ibo, m.inst})
            if (b) glDeleteBuffers(1, &b);
    };
    for (auto& row : coral_)
        for (MeshGL& m : row) freeMesh(m);
    for (MeshGL& m : fishM_) freeMesh(m);
    for (MeshGL& m : rockM_) freeMesh(m);
    freeMesh(manta_);
    for (GLuint v : {groundVao_, glowVao_, cellVao_, spriteVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, groundVbo_, groundIbo_, glowVbo_, cellVbo_, spriteVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// The camera
// =====================================================================================================

// The growth's camera (see CAMKEYS): what it frames (the polyp, then the reef) set at a place
// of the screen, at its distance, looking down a little more as it rises; it breathes a little. Its depth range
// follows its scale (near and far a fixed ratio of the distance), the world drawn relative to the eye.
void CoralReefUniverse::poseAt(float L, float t, vec3* eyeOut, vec3* anchorOut, vec3* fwdOut, vec3* rightOut, vec3* upOut) const {
    const CamKey k = camAt(std::max(0.f, L));
    const vec3 polyp = P0_ + vec3(0.f, 0.011f, 0.f);
    const vec3 mark = P0_ + vec3(0.f, 0.3f, 0.f);
    const vec3 mid = vec3(reef::TOWER_CX, -0.42f * towerAt(L), reef::TOWER_CZ);
    const vec3 A = k.anchor <= 1.f ? lerp(polyp, TARGET0, smoothstepf(0.f, 1.f, k.anchor))
                 : k.anchor <= 2.f ? lerp(TARGET0, mark, smoothstepf(0.f, 1.f, k.anchor - 1.f))
                                   : lerp(mark, mid, smoothstepf(0.f, 1.f, k.anchor - 2.f));
    const float pitch = (k.pitch + 0.5f * std::sin(t * 0.061f + 1.f)) * PI / 180.f;
    // (late, the camera drifts slowly round the place it began)
    const float yaw = 0.02f * std::sin(t * 0.043f) + 0.55f * smoothstepf(31.f, 52.f, L);
    const float tanH = std::tan(CAM_FOV * 0.5f * PI / 180.f);
    const vec3 fwd(-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch));
    const vec3 right = normalize(cross(fwd, vec3(0, 1, 0)));
    const vec3 up = cross(right, fwd);
    const vec3 dirA = normalize(fwd + right * (k.sx * tanH * aspect_) + up * (k.sy * tanH));
    vec3 eye = A - dirA * k.D;
    eye.y = std::max(eye.y, (L > 30.f ? reef::towerH(eye.x, eye.z, -std::max(2.f, towerAt(L))) : reef::groundH(eye.x, eye.z, mounds_, moundCount_)) +
                                std::min(0.2f * k.D, 6.f));
    eye.y = std::min(eye.y, reef::SURF_Y - 1.2f);
    *eyeOut = eye; *anchorOut = A; *fwdOut = fwd; *rightOut = right; *upOut = up;
}

void CoralReefUniverse::updateCamera() {
    tanH_ = std::tan(CAM_FOV * 0.5f * PI / 180.f);
    vec3 eye, A, fwd, right, up;
    poseAt(camL_, time_, &eye, &A, &fwd, &right, &up);
    eye = eye + (up * (0.35f * dive_) - fwd * (0.45f * drift_)) * camAt(std::max(0.f, camL_)).D;
    eye_ = eye;
    anchor_ = A;
    camD_ = camAt(std::max(0.f, camL_)).D;
    surfY_ = reef::SURF_Y;
    towerH_ = towerAt(camL_ + 0.8f);
    look_ = fwd;
    camR_ = right;
    camU_ = up;
    const float zn = camD_ * 0.004f;
    const float zf = std::max(camD_ * 60.f, 400.f);
    view3_ = mat4::lookAt(vec3(0.f), look_, vec3(0, 1, 0));
    proj3_ = mat4::perspective(CAM_FOV * PI / 180.f, aspect_, zn, zf);
    vp3_ = proj3_ * view3_;
}

bool CoralReefUniverse::toScreenRef(const vec3& w, vec2* ndc) const {
    const vec4 c = refVP_ * vec4(w - EYE0, 1.f);
    if (c.w <= 0.05f) return false;
    *ndc = V(c.x / c.w, c.y / c.w);
    return true;
}

vec3 CoralReefUniverse::boardToWorld(vec2 b, float dist) const {
    const float nx = (b.x - viewC_.x) / viewH_.x, ny = (b.y - viewC_.y) / viewH_.y;
    const vec3 d = normalize(look_ + camR_ * (nx * tanH_ * aspect_) + camU_ * (ny * tanH_));
    // (in the water: short of the sand where the ray comes down to it)
    const float floorY = reef::groundH(anchor_.x, anchor_.z, mounds_, moundCount_);
    if (d.y < -0.02f) dist = std::min(dist, (eye_.y - floorY - 0.045f * camD_) / -d.y);
    return eye_ + d * std::max(dist, 0.16f * camD_);
}

bool CoralReefUniverse::toScreen(const vec3& w, vec2* ndc) const {
    const vec4 c = vp3_ * vec4(w - eye_, 1.f);
    if (c.w <= 0.05f) return false;
    *ndc = V(c.x / c.w, c.y / c.w);
    return true;
}

// =====================================================================================================
// The reef's layout
// =====================================================================================================

// The outcrops, then the slots: one after the other in the order they are planted, each where it is seen best for
// its turn (the first ones in the open bands left and right of the HUD, at a middle distance; the later ones
// anywhere, farther, behind the HUD and the well), spaced out, on the rocks rather than the sand.
void CoralReefUniverse::buildReef() {
    Rng r(reefSeed_ ^ 0x5EEDC0A1ull);
    const float M[][4] = {
        {-13.f, -3.f, 3.2f, 1.5f},  {-9.5f, -9.f, 3.5f, 1.9f}, {-17.5f, -12.f, 4.2f, 2.2f}, {-7.f, 1.5f, 2.f, 0.9f},
        {12.5f, -2.f, 3.3f, 1.6f},  {9.f, -10.f, 3.8f, 2.1f},  {17.5f, -9.f, 4.2f, 2.f},
        {-4.f, -24.f, 6.f, 2.6f},   {6.f, -27.f, 7.f, 3.f},    {-17.f, -26.f, 6.f, 2.4f},   {19.f, -25.f, 6.f, 2.6f},
        {0.5f, -13.f, 2.4f, 0.9f},  {-11.f, 7.5f, 2.2f, 0.7f},
    };
    moundCount_ = 0;
    for (auto& m : M) {
        reef::Mound& o = mounds_[moundCount_++];
        o.x = m[0] + r.range(-0.8f, 0.8f);
        o.z = m[1] + r.range(-0.8f, 0.8f);
        o.r = m[2] * r.range(0.9f, 1.1f);
        o.h = m[3] * r.range(0.85f, 1.15f);
        o.seed = r.uniform();
    }
    // boulders: a few on each outcrop, some loose in the sand
    rocks_.clear();
    for (int i = 0; i < moundCount_; i++) {
        const reef::Mound& m = mounds_[i];
        const int n = 2 + (int)(m.r * 0.8f);
        for (int k = 0; k < n; k++) {
            Rock b;
            const float a = r.range(0.f, TAU), d = m.r * std::sqrt(r.uniform()) * 0.75f;
            b.p = vec3(m.x + std::cos(a) * d, 0.f, m.z + std::sin(a) * d);
            b.size = m.r * r.range(0.35f, 0.62f) * (1.f - 0.4f * d / m.r);
            b.p.y = reef::groundH(b.p.x, b.p.z, mounds_, moundCount_) - 0.12f * b.size;
            b.yaw = r.range(0.f, TAU);
            b.seed = r.uniform();
            b.tmpl = r.irange(0, reef::TEMPLATES - 1);
            b.birth = -1.f;
            b.grow = 1.f;
            rocks_.push_back(b);
        }
    }
    P0_.y = reef::groundH(P0_.x, P0_.z, mounds_, moundCount_);
    for (int k = 0; k < 10; k++) {
        Rock b;
        b.p = vec3(r.range(-22.f, 22.f), 0.f, r.range(-18.f, 8.f));
        if (length(vec3(b.p.x - P0_.x, 0.f, b.p.z - P0_.z)) < 3.5f) continue;
        b.size = r.range(0.5f, 1.3f);
        b.p.y = reef::groundH(b.p.x, b.p.z, mounds_, moundCount_) - 0.12f * b.size;
        b.yaw = r.range(0.f, TAU);
        b.seed = r.uniform();
        b.tmpl = r.irange(0, reef::TEMPLATES - 1);
        b.birth = -1.f;
        b.grow = 1.f;
        rocks_.push_back(b);
    }
    // the camera's path while it pulls back from the polyp to the reef: nothing big grows in its line of sight
    path_.clear();
    for (int i = 0; i <= 40; i++) {
        vec3 e, a, fw, rt, up;
        poseAt(6.f + 26.f * i / 40.f, 0.f, &e, &a, &fw, &rt, &up);
        path_.push_back(e);
        path_.push_back(a);
    }
    auto inSight = [&](float x, float z, float size) {
        for (size_t i = 0; i + 1 < path_.size(); i += 2) {
            const vec3 e = path_[i], a = path_[i + 1];
            const vec3 ab(a.x - e.x, 0.f, a.z - e.z);
            const float l2 = std::max(dot(ab, ab), 1e-6f);
            const float t = std::clamp(((x - e.x) * ab.x + (z - e.z) * ab.z) / l2, 0.f, 0.85f);
            const float dx = x - (e.x + ab.x * t), dz = z - (e.z + ab.z * t);
            if (dx * dx + dz * dz < sqf(0.35f * length(ab) * 0.15f + 0.6f * size + 0.3f)) return true;
        }
        return false;
    };
    // ---- the beginning, at the polyp's scale: a pebble in the sand, the polyp on it, its buds, the coral head
    // (it covers them: the colony), little corals around it; then the bommie's boulders rising around the head and
    // the colonies on them
    slots_.clear();
    {
        Rock peb;
        peb.p = P0_ - vec3(0.f, 0.003f, 0.f);
        peb.p = P0_ - vec3(0.f, 0.005f, 0.f);
        peb.size = 0.022f; peb.yaw = 0.7f; peb.seed = 0.3f; peb.tmpl = 1; peb.birth = -1.f; peb.grow = 1.f;
        rocks_.push_back(peb);
        auto micro = [&](int kind, vec3 at, float size, float birth, int pair) {
            Slot sl;
            sl.p = at; sl.size = size; sl.kind = kind; sl.tmpl = r.irange(0, reef::TEMPLATES - 1);
            sl.yaw = r.range(0.f, TAU); sl.seed = r.uniform(); sl.birth = birth;
            sl.colIdx[0] = PAIRS[kind][pair][0]; sl.colIdx[1] = PAIRS[kind][pair][1];
            sl.startT = -1.f; sl.grow = 0.f; sl.awaiting = false;
            slots_.push_back(sl);
        };
        const float top = P0_.y - 0.005f + 0.022f * 0.42f;
        micro(reef::K_ANEMONE, vec3(P0_.x, top, P0_.z), 0.022f, -1.f, 0);
        for (int i = 0; i < 6; i++) {
            const float a = TAU * i / 6.f + r.range(-0.4f, 0.4f), d = r.range(0.007f, 0.02f);
            const vec3 at(P0_.x + std::cos(a) * d, 0.f, P0_.z + std::sin(a) * d);
            const float y = d < 0.011f ? top - 0.002f : reef::groundH(at.x, at.z, mounds_, moundCount_);
            micro(reef::K_ANEMONE, vec3(at.x, y, at.z), r.range(0.006f, 0.009f), 0.8f + 0.7f * i, i % 4);
        }
        micro(reef::K_BRAIN, P0_ - vec3(0.f, 0.006f, 0.f), 0.13f, 5.f, 0);
        const float around[5][4] = {{0.11f, 0.03f, 0.1f, 1.f}, {-0.1f, 0.05f, 0.05f, 5.f}, {0.02f, -0.11f, 0.13f, 6.f},
                                    {-0.06f, -0.08f, 0.04f, 5.f}, {0.08f, 0.09f, 0.11f, 3.f}};
        for (int i = 0; i < 5; i++) {
            const vec3 at(P0_.x + around[i][0], 0.f, P0_.z + around[i][1]);
            micro((int)around[i][3], vec3(at.x, reef::groundH(at.x, at.z, mounds_, moundCount_) - 0.004f, at.z), around[i][2], 6.f + 0.6f * i, i % 4);
        }
        // the bommie: boulders rising around the head
        for (int i = 0; i < 5; i++) {
            Rock b;
            const float a = PI * (1.05f + 0.9f * i / 4.f) + r.range(-0.15f, 0.15f), d = r.range(0.45f, 0.8f);
            b.p = vec3(P0_.x + std::cos(a) * d, 0.f, P0_.z + std::sin(a) * d);
            b.size = r.range(0.55f, 0.95f);
            b.p.y = reef::groundH(b.p.x, b.p.z, mounds_, moundCount_) - 0.12f * b.size;
            b.yaw = r.range(0.f, TAU); b.seed = r.uniform(); b.tmpl = r.irange(0, reef::TEMPLATES - 1);
            b.birth = 9.f + 0.9f * i;
            b.grow = 0.f;
            rocks_.push_back(b);
        }
        for (int i = 0; i < 18; i++) {
            const float birth = 10.5f + 6.f * i / 17.f;
            const int kinds[6] = {reef::K_BRAIN, reef::K_STAG, reef::K_ANEMONE, reef::K_FAN, reef::K_SOFT, reef::K_SPONGE};
            const int kind = kinds[i % 6];
            for (int tries = 0; tries < 30; tries++) {
                const float a = r.range(PI * 0.9f, PI * 2.1f), d = r.range(0.2f, 1.15f);
                const float x = P0_.x + std::cos(a) * d, z = P0_.z + std::sin(a) * d;
                const float size = r.range(0.14f, 0.42f) * (kind == reef::K_FAN ? 1.4f : 1.f);
                bool ok = true;
                for (const Slot& o : slots_)
                    if (o.size > 0.05f && length(vec3(o.p.x - x, 0.f, o.p.z - z)) < (o.size + size) * 0.4f) { ok = false; break; }
                if (!ok) continue;
                micro(kind, vec3(x, surfaceAt(x, z) - 0.05f * size, z), size, birth, r.irange(0, 3));
                break;
            }
        }
    }
    int lastKind[reef::K_COUNT];
    for (int& k : lastKind) k = -1;
    for (int i = 0; i < N_SLOTS + 24; i++) {
        Slot s;
        const float t = (float)i / (N_SLOTS - 1);
        s.birth = i < N_SLOTS ? 16.f + 18.f * t : 50.f + (i - N_SLOTS + 1) * 1.5f;
        // its form: a newly unlocked one comes at once; else a weighted pick among the unlocked
        int kind = -1;
        for (int k = 0; k < reef::K_COUNT; k++)
            if (KINDS[k].unlock * 0.36f + 16.f <= s.birth && lastKind[k] < 0) { kind = k; break; }
        if (kind < 0) {
            float w[reef::K_COUNT];
            for (int k = 0; k < reef::K_COUNT; k++) {
                const float un = KINDS[k].unlock * 0.36f + 16.f;
                w[k] = un <= s.birth ? KINDS[k].weight : 0.f;
                // a form just unlocked comes a little more often for a while
                if (un <= s.birth && s.birth < un + 2.5f) w[k] *= 1.8f;
            }
            kind = r.weighted(w);
        }
        lastKind[kind] = i;
        s.kind = kind;
        s.tmpl = r.irange(0, reef::TEMPLATES - 1);
        const KindDef& kd = KINDS[kind];
        s.size = r.range(kd.sizeLo, kd.sizeHi) * (0.85f + 0.35f * saturate((s.birth - 16.f) / 18.f));
        s.yaw = r.range(0.f, TAU);
        s.seed = r.uniform();
        const int pr = r.irange(0, 3);
        s.colIdx[0] = PAIRS[kind][pr][0];
        s.colIdx[1] = PAIRS[kind][pr][1];
        // where: the best of a few candidates
        const float early = 1.f - smoothstepf(0.f, 0.45f, t);
        float best = -1.f;
        vec3 bestP(0.f);
        for (int c = 0; c < 48; c++) {
            const float x = r.range(-27.f, 27.f), z = r.range(-31.f, 10.f);
            if (inSight(x, z, s.size)) continue;
            const float y = surfaceAt(x, z);
            const vec3 p(x, y, z);
            vec2 a, b;
            if (!toScreenRef(p, &a) || !toScreenRef(p + vec3(0.f, s.size, 0.f), &b)) continue;
            if (std::fabs(a.x) > 1.02f || a.y < -1.15f || b.y > 0.9f) continue;
            const float ax = std::fabs(a.x);
            const float vis = ax > 0.56f ? 1.f : ax > 0.27f ? 0.5f : 0.3f;
            const float dist = length(p - EYE0);
            const float distPref = lerpf(1.f, std::exp(-sqf((dist - 14.f) / 6.f)), early);
            const float rock = reef::rockAt(x, z, mounds_, moundCount_);
            const float rockPref = 1.f + (kd.rockPref - 1.f) * rock;
            float space = 1.f;
            for (const Slot& o : slots_) {
                const float d = length(vec3(o.p.x - x, 0.f, o.p.z - z));
                const float need = (o.size + s.size) * 0.42f;
                if (d < need) { space = 0.f; break; }
                space = std::min(space, smoothstepf(need, need * 1.8f, d) * 0.6f + 0.4f);
            }
            const float score = std::pow(vis, lerpf(0.8f, 3.f, early)) * distPref * rockPref * space * r.range(0.6f, 1.f);
            if (score > best) { best = score; bestP = p; }
        }
        if (best <= 0.f) continue;
        s.p = bestP - vec3(0.f, 0.06f * s.size, 0.f);
        s.startT = -1.f;
        s.grow = 0.f;
        s.awaiting = false;
        slots_.push_back(s);
    }
    // the tower's colonies: on the summit's rim, then down its flanks as it stacks up (each born when the tower's foot
    // has sunk past it), bigger and bigger down the flanks (colonies on colonies): brain corals and sponges on the
    // cliffs, table corals jutting from the ledges like shelves, sea fans on the edges, staghorn thickets
    {
        const size_t first = slots_.size();
        auto clash = [&](float x, float z, float y, float need) {
            for (size_t k = first; k < slots_.size(); k++) {
                const Slot& o = slots_[k];
                const float dx = o.p.x - x, dz = o.p.z - z, dy = o.p.y - y;
                if (dx * dx + dz * dz + dy * dy < sqf((o.size + need) * 0.3f)) return true;
            }
            return false;
        };
        for (int i = 0, made = 0; i < 20000 && made < 3400; i++) {
            const float a = r.range(0.f, TAU);
            const float depth = reef::TOWER_MAX * std::pow(r.uniform(), 1.35f);
            const float rr = reef::towerR(depth, a) + r.range(-1.f, 1.f);
            const float x = reef::TOWER_CX + std::cos(a) * rr, z = reef::TOWER_CZ + std::sin(a) * rr;
            const float y = reef::towerH(x, z, -reef::TOWER_MAX - 50.f);
            if (y > -0.6f && length(vec3(x - reef::TOWER_CX, 0.f, z - reef::TOWER_CZ)) < reef::TOP_R + 2.f) continue;
            // the slope there: ledges take the tables, cliffs the brains and sponges
            const float e = 1.5f;
            const float gx = reef::towerH(x + e, z, -reef::TOWER_MAX - 50.f) - reef::towerH(x - e, z, -reef::TOWER_MAX - 50.f);
            const float gz = reef::towerH(x, z + e, -reef::TOWER_MAX - 50.f) - reef::towerH(x, z - e, -reef::TOWER_MAX - 50.f);
            const float slope = std::sqrt(gx * gx + gz * gz) / (2.f * e);
            const bool ledge = slope < 0.9f;
            float w[reef::K_COUNT] = {0.f};
            w[reef::K_BRAIN] = ledge ? 0.6f : 1.4f;
            w[reef::K_STAG] = ledge ? 0.35f : 0.12f;   // (the heaviest form: rarer on the tower)
            w[reef::K_TABLE] = ledge ? 1.6f : 0.25f;
            w[reef::K_FAN] = 0.7f;
            w[reef::K_SPONGE] = ledge ? 0.5f : 0.9f;
            w[reef::K_SOFT] = 0.3f;
            Slot sl;
            sl.kind = r.weighted(w);
            sl.size = r.range(2.6f, 5.5f) * (1.f + depth / 55.f) * (sl.kind == reef::K_TABLE ? 1.25f : 1.f);
            if (clash(x, z, y, sl.size)) continue;
            sl.tmpl = r.irange(0, reef::TEMPLATES - 1);
            // (half sunk in the rock, so the stack reads as colonies grown on colonies)
            sl.p = vec3(x, y - (ledge ? 0.08f : 0.3f) * sl.size, z);
            sl.yaw = r.range(0.f, TAU);
            sl.seed = r.uniform();
            const int pr = r.irange(0, 3);
            sl.colIdx[0] = PAIRS[sl.kind][pr][0];
            sl.colIdx[1] = PAIRS[sl.kind][pr][1];
            sl.birth = depth < 6.f ? r.range(30.5f, 33.f) : towerLineFor(depth + 3.f) + r.range(0.f, 0.6f);
            sl.startT = -1.f; sl.grow = 0.f; sl.awaiting = false;
            slots_.push_back(sl);
            made++;
        }
    }
    std::sort(slots_.begin(), slots_.end(), [](const Slot& a, const Slot& b) { return a.birth < b.birth; });
    reefDirty_ = false;
}

float CoralReefUniverse::surfaceAt(float x, float z) const {
    float y = reef::groundH(x, z, mounds_, moundCount_);
    for (const Rock& b : rocks_) {
        const float R = b.size * 0.5f;
        const float dx = (x - b.p.x) / 1.1f, dz = z - b.p.z;
        const float d2 = (dx * dx + dz * dz) / (R * R);
        if (d2 < 1.f) y = std::max(y, b.p.y + b.size * (0.1f + 0.36f * std::sqrt(1.f - d2)));
    }
    return y;
}

void CoralReefUniverse::buildSchools() {
    schools_.clear();
    Rng r(reefSeed_ ^ 0xF15Bull);
    for (const SchoolDef& d : SCHOOLS) {
        School s;
        s.species = d.species;
        s.appear = d.appear;
        s.maxCount = d.count;
        s.size = d.size;
        s.zoneX = V(d.x0, d.x1);
        s.path = r.range(0.f, 100.f);
        s.speed = SPECIES_SPEED[d.species];
        s.center = vec3(0.5f * (d.x0 + d.x1), 2.5f, -6.f);
        s.target = s.center;
        schools_.push_back(s);
    }
    fish_.clear();
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void CoralReefUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && !t.minor && t.brightness > 0.58f) v = 0;          // the lagoon for bright, major songs
        else if (t.hasSong && t.minor && t.brightness < 0.4f) v = 3;       // the night dive for dark ones
        else if (t.hasSong && t.bassWeight > 0.62f && t.dynamics > 0.6f) v = 2; // the outer reef for deep, dynamic ones
        else {
            static const int BY_HUE[] = {1, 0, 2, 1, 0, 2};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0xC0BA1ull);
    if (reefSeed_ == 0) { reefSeed_ = seed_; reefDirty_ = true; }
    if (seconds <= 0.f) palSet_ = false;
}

void CoralReefUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropMode_ = false;
    (void)impact;
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
}

void CoralReefUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

void CoralReefUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

vec2 CoralReefUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints CoralReefUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = lerp(p.water, p.deep, 0.4f);
    h.light = false;
    h.ink = p.ink;
    h.accent = p.accent;
    h.styles = TS_INK | TS_RIPPLE | TS_LUMA;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void CoralReefUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Spawn:
        // the new piece's colony and its four blocks' seeds (they stay theirs once it locks)
        for (int i = 0; i < 4; i++) pieceSeeds_[i] = 0.02f + 0.96f * blockRng_.uniform();
        pieceColony_ = 0.02f + 0.96f * blockRng_.uniform();
        break;
    case GameEvent::Lock: {
        int minx = 99, maxy = -99;
        for (const auto& c : ev.cells) { minx = std::min(minx, c.x); maxy = std::max(maxy, c.y); }
        for (size_t i = 0; i < ev.cells.size() && i < 4; i++) {
            const auto& c = ev.cells[i];
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            blockSeeds_[c.y][c.x] = pieceSeeds_[i];
            colonies_[c.y][c.x] = pieceColony_;
            seedType_[c.y][c.x] = (int8_t)(c.type + 1);
            places_[c.y][c.x] = (int8_t)(std::clamp(c.x - minx, 0, 7) + 8 * std::clamp(maxy - c.y, 0, 7));
        }
        break;
    }
    case GameEvent::HardDrop: {
        // a few tiny bubbles squeezed out from under the piece, at its sides
        float yLo = 99.f, xLo = 99.f, xHi = -99.f;
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            yLo = std::min(yLo, p.y);
        }
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > yLo + 0.01f) continue;
            xLo = std::min(xLo, p.x);
            xHi = std::max(xHi, p.x);
        }
        if (xLo > xHi) break;
        for (int side = 0; side < 2; side++)
            for (int i = 0; i < 3; i++) {
                Puff q;
                const float sx = side == 0 ? -1.f : 1.f;
                q.p = V((side == 0 ? xLo - 0.5f : xHi + 0.5f) + sx * rng_.range(0.f, 0.15f), yLo - 0.42f + rng_.range(0.f, 0.15f));
                q.v = V(sx * rng_.range(0.4f, 0.9f), rng_.range(0.2f, 0.6f));
                q.age = -rng_.range(0.f, 0.08f);
                q.life = rng_.range(0.5f, 0.8f);
                q.size = rng_.range(0.06f, 0.11f);
                puffs_.push_back(q);
            }
        break;
    }
    case GameEvent::Clear: {
        // the row shrinks away; it bursts into bubbles in the water behind the well (see render: the seeds)
        std::vector<vec2> pts;
        bool rows[Game::H] = {};
        for (auto& cl : ev.cells)
            if (cl.y >= 0 && cl.y < Game::H) rows[cl.y] = true;
        auto ours = [&](int x, int y, int type) {
            return x >= 0 && x < Game::W && y >= 0 && y < Game::H && blockSeeds_[y][x] > 0.f && seedType_[y][x] == type + 1;
        };
        for (auto& cl : ev.cells) {
            if (cl.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)cl.x, (float)cl.y);
            // (it keeps its seed, and its colony with the blocks of the rows cleared with it)
            float sd = 0.f, co = 0.5f;
            int nb = 0, place = 0;
            if (ours(cl.x, cl.y, cl.type)) {
                sd = blockSeeds_[cl.y][cl.x];
                co = colonies_[cl.y][cl.x];
                place = places_[cl.y][cl.x];
                auto with = [&](int x, int y) { return ours(x, y, cl.type) && rows[y] && colonies_[y][x] == co; };
                nb = (with(cl.x, cl.y - 1) ? 1 : 0) | (with(cl.x, cl.y + 1) ? 2 : 0) | (with(cl.x - 1, cl.y) ? 4 : 0) |
                     (with(cl.x + 1, cl.y) ? 8 : 0);
            }
            if (sd <= 0.f) sd = 0.02f + 0.96f * blockRng_.uniform();
            fading_.push_back({p.x, p.y, std::clamp(cl.type, 0, 6), 0.f, sd, co, nb, place});
            pts.push_back(V(p.x, p.y));
            // the bubbles burst out of the row sideways behind the well, then rise
            const float dist = camD_ * rng_.range(0.7f, 0.95f), sc = camD_ / 15.6f;
            const float side = p.x + rng_.range(-0.6f, 0.6f) < 0.f ? -1.f : 1.f;
            for (int i = 0; i < 3 && (int)bubbles_.size() < MAX_BUBBLES; i++) {
                Bubble b;
                b.p = boardToWorld(V(p.x + rng_.range(-0.45f, 0.45f), p.y + rng_.range(-0.45f, 0.45f)), dist + rng_.range(-1.f, 1.f));
                b.v = (camR_ * (side * rng_.range(1.f, 2.8f) * (0.6f + std::fabs(p.x) / 6.f)) +
                       vec3(rng_.range(-0.3f, 0.3f), rng_.range(0.3f, 1.2f), rng_.range(-0.5f, 0.3f))) * sc;
                b.r = rng_.range(0.0035f, 0.008f) * (i == 0 ? 1.4f : 1.f) * std::min(length(b.p - eye_), 30.f);
                b.age = -rng_.range(0.f, 0.25f);
                b.life = 9.f;
                b.seed = rng_.uniform();
                bubbles_.push_back(b);
            }
        }
        {   // the blocks' seeds follow their rows down
            float ns[Game::H][Game::W] = {}, nc[Game::H][Game::W] = {};
            int8_t nt[Game::H][Game::W] = {}, np[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(ns[dst], blockSeeds_[y], sizeof(blockSeeds_[y]));
                std::memcpy(nc[dst], colonies_[y], sizeof(colonies_[y]));
                std::memcpy(nt[dst], seedType_[y], sizeof(seedType_[y]));
                std::memcpy(np[dst], places_[y], sizeof(places_[y]));
                dst--;
            }
            std::memcpy(blockSeeds_, ns, sizeof(blockSeeds_));
            std::memcpy(colonies_, nc, sizeof(colonies_));
            std::memcpy(seedType_, nt, sizeof(seedType_));
            std::memcpy(places_, np, sizeof(places_));
        }
        if (!pts.empty()) {
            clearPts_.insert(clearPts_.end(), pts.begin(), pts.end());
            clearPending_ = true;
        }
        if (ev.count >= 4) { // a quad: the schools gather and swirl together, on one side of the well or the other
            swirlT_ = 0.f;
            swirlSide_ = -swirlSide_;
            swirlC_ = vec3(10.5f * swirlSide_, 3.3f, 1.5f);
        }
        break;
    }
    case GameEvent::TopOut: overT_ = 0.f; break;
    default: break;
    }
}

// =====================================================================================================
// Fish
// =====================================================================================================

// Boids: each fish keeps away from its nearest neighbours, matches their heading, keeps with them and with its
// school's target, stays over the reef and under the surface; its heading turns at a limited rate (smooth arcs,
// never a snap), its speed eases. The schools' targets wander along their stretch of reef, their height following
// the melody, their pace the energy.
void CoralReefUniverse::updateFish(float dt, float energy, float melody, float swirl) {
    const float L = shownLines_;
    // the schools' size with the reef; new fish swim in from out of view
    for (int si = 0; si < (int)schools_.size(); si++) {
        School& s = schools_[si];
        int want = 0;
        if (L >= s.appear) {
            const float k = saturate((L - s.appear) / 22.f);
            want = (int)std::lround(lerpf(std::min(8.f, (float)s.maxCount), (float)s.maxCount, k));
        }
        int have = 0;
        for (const Fish& f : fish_) have += f.school == si;
        while (have < want) {
            Fish f;
            const float side = (s.zoneX.x + s.zoneX.y) < 0.f ? -1.f : 1.f;
            f.p = vec3(side * rng_.range(30.f, 38.f), rng_.range(1.5f, 4.5f), rng_.range(-14.f, -4.f));
            if (fish_.empty() || shownLinesFirst_) f.p = s.target + vec3(rng_.range(-3.f, 3.f), rng_.range(-1.f, 1.f), rng_.range(-3.f, 3.f));
            f.v = vec3(-side, 0.f, 0.f) * s.speed;
            f.dir = normalize(f.v);
            f.phase = rng_.range(0.f, TAU);
            f.size = s.size * rng_.range(0.85f, 1.15f);
            f.bank = 0.f;
            f.speed = s.speed;
            f.seed = rng_.uniform();
            f.school = si;
            f.home = -1;
            f.in = 1.f;
            fish_.push_back(f);
            have++;
        }
        while (have > want) { // (a new game: they swim off; simply removed)
            for (size_t i = fish_.size(); i-- > 0;)
                if (fish_[i].school == si) { fish_.erase(fish_.begin() + (long)i); break; }
            have--;
        }
        // the target wanders
        const float pace = 0.6f + 0.9f * energy;
        s.path += dt * pace;
        const float mid = 0.5f * (s.zoneX.x + s.zoneX.y), half = 0.5f * (s.zoneX.y - s.zoneX.x);
        vec3 tgt(mid + half * std::sin(s.path * 0.11f + si * 1.7f), 1.6f + 2.6f * melody + 0.7f * std::sin(s.path * 0.23f + si),
                 1.5f + 3.5f * std::sin(s.path * 0.083f + si * 2.3f));
        tgt.y = std::max(tgt.y, reef::groundH(tgt.x, tgt.z, mounds_, moundCount_) + 1.2f);
        if (swirl > 0.f) tgt = lerp(tgt, swirlC_, swirl);
        s.target = tgt;
    }
    // the coral head's goby, the bommie's little fish
    {
        int goby = 0, small = 0;
        for (const Fish& f : fish_) { goby += f.school == -2 && f.home == -2; small += f.school == -2 && f.home == -3; }
        const int wantG = L >= 6.f ? 1 : 0, wantS = L >= 11.f ? (L >= 14.f ? 7 : 4) : 0;
        auto add = [&](int home, float size) {
            Fish f;
            f.p = P0_ + vec3(rng_.range(-0.3f, 0.3f), 0.1f, rng_.range(-0.3f, 0.3f)) * (home == -2 ? 0.3f : 2.f);
            f.v = vec3(0.f); f.dir = vec3(1, 0, 0); f.phase = rng_.range(0.f, TAU); f.size = size;
            f.bank = 0.f; f.speed = 0.f; f.seed = rng_.uniform(); f.school = -2; f.home = home; f.in = 1.f;
            fish_.push_back(f);
        };
        for (; goby < wantG; goby++) add(-2, 0.032f);
        for (; small < wantS; small++) add(-3, rng_.range(0.08f, 0.11f));
        if (goby > wantG || small > wantS)
            fish_.erase(std::remove_if(fish_.begin(), fish_.end(), [&](const Fish& f) {
                return f.school == -2 && ((f.home == -2 && wantG == 0) || (f.home == -3 && wantS == 0)); }), fish_.end());
    }
    // clownfish: two in each grown anemone (the reef's, not the polyps')
    for (int i = 0; i < (int)slots_.size(); i++) {
        const Slot& sl = slots_[i];
        if (sl.kind != reef::K_ANEMONE || sl.grow < 0.9f || sl.size < 0.5f) continue;
        int have = 0;
        for (const Fish& f : fish_) have += f.home == i;
        for (; have < 2; have++) {
            Fish f;
            f.p = sl.p + vec3(rng_.range(-0.3f, 0.3f), 0.4f * sl.size, rng_.range(-0.3f, 0.3f));
            f.v = vec3(0.2f, 0.f, 0.f);
            f.dir = vec3(1, 0, 0);
            f.phase = rng_.range(0.f, TAU);
            f.size = 0.34f * rng_.range(0.9f, 1.1f) * (have == 0 ? 1.15f : 0.9f);
            f.bank = 0.f;
            f.speed = 0.4f;
            f.seed = rng_.uniform();
            f.school = -1;
            f.home = i;
            f.in = 1.f;
            fish_.push_back(f);
        }
    }
    fish_.erase(std::remove_if(fish_.begin(), fish_.end(),
                               [&](const Fish& f) { return f.school == -1 && (f.home < 0 || f.home >= (int)slots_.size() || slots_[f.home].grow < 0.5f); }),
                fish_.end());

    const int n = (int)fish_.size();
    const float sub = std::min(dt, 0.05f);
    int steps = std::max(1, (int)std::ceil(dt / 0.05f));
    steps = std::min(steps, 3);
    for (int st = 0; st < steps; st++) {
        for (int i = 0; i < n; i++) {
            Fish& f = fish_[i];
            vec3 want;
            float spd;
            if (f.school == -2) {
                // the goby by its coral head, the little fish round the bommie: hovering, darting about
                const float R = f.home == -2 ? 0.1f : 0.75f, sp = f.home == -2 ? 0.07f : 0.5f;
                const float a = time_ * (0.35f + 0.25f * f.seed) * (f.home == -2 ? 1.f : 0.6f) + f.seed * 20.f;
                const vec3 hp = P0_ + vec3(std::cos(a) * R, (f.home == -2 ? 0.05f : 0.45f) + 0.3f * R * std::sin(a * 1.3f), std::sin(a) * R);
                want = (hp - f.p) * (2.f / R);
                spd = std::min(sp, length(hp - f.p) * 1.5f + sp * 0.3f);
            } else if (f.school < 0) {
                // a clownfish hovers in its anemone's tentacles, darting about a little
                const Slot& sl = slots_[f.home];
                const float a = time_ * (0.5f + 0.3f * f.seed) + f.seed * 20.f;
                const vec3 hp = sl.p + vec3(std::cos(a) * 0.32f * sl.size, (0.32f + 0.08f * std::sin(a * 1.7f)) * sl.size, std::sin(a) * 0.32f * sl.size);
                want = (hp - f.p) * 1.4f;
                spd = std::min(0.9f, length(want));
            } else {
                const School& s = schools_[f.school];
                vec3 sep(0.f), ali(0.f), coh(0.f);
                int cnt = 0;
                const float R = 2.4f, Rs = f.size * 2.6f;
                for (int j = 0; j < n; j++) {
                    if (j == i || fish_[j].school != f.school) continue;
                    const vec3 d = fish_[j].p - f.p;
                    const float d2 = dot(d, d);
                    if (d2 > R * R) continue;
                    const float dd = std::sqrt(d2) + 1e-4f;
                    if (dd < Rs) sep += d * (-(Rs - dd) / (dd * Rs));
                    ali += fish_[j].v;
                    coh += fish_[j].p;
                    cnt++;
                }
                want = (s.target - f.p) * (0.12f + 0.25f * swirl);
                const float tl = length(want), cap = 1.2f + 1.6f * swirl;
                if (tl > cap) want = want * (cap / tl);
                if (cnt > 0) {
                    ali = ali / (float)cnt;
                    coh = coh / (float)cnt - f.p;
                    want += normalize(ali) * 0.9f + coh * 0.35f + sep * 2.2f;
                }
                // a quad: they wheel round the swirl's axis
                if (swirl > 0.f) {
                    const vec3 rd = f.p - swirlC_;
                    want += normalize(vec3(-rd.z, 0.f, rd.x) + vec3(0.f, 0.1f * std::sin(time_ + f.seed * 9.f), 0.f)) * (1.3f * swirl);
                }
                // over the reef, under the surface, away from the camera
                const float gh = reef::groundH(f.p.x, f.p.z, mounds_, moundCount_);
                if (f.p.y < gh + 1.f) want.y += (gh + 1.f - f.p.y) * 2.5f;
                if (f.p.y > reef::SURF_Y - 1.2f) want.y -= (f.p.y - (reef::SURF_Y - 1.2f)) * 2.f;  // (the shallows' own depth)
                if (f.p.z > 7.f) want.z -= (f.p.z - 7.f) * 1.5f;
                if (f.p.z < -30.f) want.z += 1.f;
                spd = s.speed * (0.75f + 0.75f * energy) * (1.f + 0.35f * swirl) * (0.9f + 0.2f * f.seed);
            }
            // steer: the heading turns toward the wish at a limited rate, the speed eases
            vec3 wd = length(want) > 1e-4f ? normalize(want) : f.dir;
            wd.y *= 0.6f; // fish swim level more than they climb
            wd = normalize(wd);
            const float maxTurn = (f.school < 0 ? 3.f : 1.5f) * sub;
            const float cosA = std::clamp(dot(f.dir, wd), -1.f, 1.f);
            const float ang = std::acos(cosA);
            vec3 nd;
            if (ang <= maxTurn) nd = wd;
            else {
                const vec3 ax = cross(f.dir, wd);
                const float al = length(ax);
                const vec3 k = al > 1e-5f ? ax / al : vec3(0, 1, 0);
                nd = normalize(f.dir * std::cos(maxTurn) + cross(k, f.dir) * std::sin(maxTurn) + k * (dot(k, f.dir) * (1.f - std::cos(maxTurn))));
            }
            // bank into the turn (eased)
            const float turn = cross(f.dir, nd).y / std::max(sub, 1e-4f);
            f.bank += (std::clamp(-turn * 0.35f, -0.5f, 0.5f) - f.bank) * std::min(1.f, sub * 3.f);
            f.dir = nd;
            f.speed += (spd - f.speed) * std::min(1.f, sub * 1.5f);
            f.v = f.dir * f.speed;
            f.p += f.v * sub;
            f.phase += sub * (5.f + f.speed * 7.f / std::max(0.05f, f.size * 2.f)) / std::max(0.3f, f.size * 2.5f);
        }
    }
}

// =====================================================================================================
// The drop
// =====================================================================================================

// A bundle of eggs leaves a coral's top and rises slowly, wavering (upside-down snow).
void CoralReefUniverse::spawnEgg(const Slot& sl, float delay) {
    if ((int)eggs_.size() >= MAX_EGGS) return;
    static const unsigned COLS[5] = {0xffb4c8, 0xffc8a0, 0xfff0d2, 0xff9ab4, 0xffd6b8};
    Egg e;
    const float a = rng_.range(0.f, TAU), d = rng_.range(0.f, 0.35f) * sl.size;
    e.p = sl.p + vec3(std::cos(a) * d, sl.size * rng_.range(0.35f, 0.75f), std::sin(a) * d);
    e.r = rng_.range(0.06f, 0.1f) * std::clamp(camD_ / 15.6f, 0.002f, 1.f) * (camL_ > 36.f ? std::max(1.f, std::pow(camD_ / 15.6f, 0.85f)) : 1.f);
    e.age = delay;
    e.life = rng_.range(18.f, 30.f);
    e.seed = rng_.uniform();
    e.fade = 1.f;
    e.col = lin(COLS[rng_.irange(0, 4)]);
    eggs_.push_back(e);
}

// The mantas' loop: a slow, wide figure in one side band and the open water above it, seen from the side, coming
// near and going away (placed in screen space so it stays in view on a 16:9 screen); `enter` 0 .. 1 glides it in.
vec3 CoralReefUniverse::mantaAt(float t, float enter) const {
    const float w = TAU / 26.f;
    const float nx = mantaSide_ * (0.8f + 0.07f * std::sin(w * t) + 0.9f * (1.f - enter));
    const float ny = 0.32f + 0.2f * std::sin(2.f * w * t + 0.6f);
    const float dist = (21.f + 4.f * std::cos(w * t)) * camD_ / 15.6f * lerpf(1.f, 0.62f, smoothstepf(33.f, 40.f, camL_));
    const vec3 d = normalize(look_ + camR_ * (nx * tanH_ * aspect_) + camU_ * (ny * tanH_));
    vec3 p = eye_ + d * dist;
    p.y = camL_ > 31.f ? std::clamp(p.y, -towerH_ + 10.f, reef::SURF_Y - 3.f) : std::max(p.y, 2.2f);
    return p;
}

// =====================================================================================================
// Board
// =====================================================================================================

void CoralReefUniverse::cell(float x, float y, float half, const vec3& tint, float alpha, int mode, float erase, int type,
                             float seed, float colony, int nb, int place) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)(nb + 16 * std::clamp(type, 0, 6) + 128 * (place & 63))}, {tint.x, tint.y, tint.z, alpha},
                      {(float)mode, erase, colony, seed}});
}

void CoralReefUniverse::buildBoard(const Game& g) {
    auto tintOf = [&](int t) { return pal_.piece[std::clamp(t, 0, 6)]; };
    const float darken = 1.f - 0.45f * smoothstepf(0.f, 1.f, over_);
    // a piece's block's neighbours among its own four
    auto pieceMask = [](const int (&cs)[4][2], int i) {
        int m = 0;
        for (int j = 0; j < 4; j++) {
            const int ddx = cs[j][0] - cs[i][0], ddy = cs[j][1] - cs[i][1];
            if (ddx == 0 && ddy == -1) m |= 1;
            if (ddx == 0 && ddy == 1) m |= 2;
            if (ddx == -1 && ddy == 0) m |= 4;
            if (ddx == 1 && ddy == 0) m |= 8;
        }
        return m;
    };
    // its place in the piece (column, row up from the lowest)
    auto piecePlace = [](const int (&cs)[4][2], int i) {
        int minx = 99, maxy = -99;
        for (auto& c : cs) { minx = std::min(minx, c[0]); maxy = std::max(maxy, c[1]); }
        return std::clamp(cs[i][0] - minx, 0, 7) + 8 * std::clamp(maxy - cs[i][1], 0, 7);
    };
    // a block of the board: its seeds and its colony's; the blocks it never saw lock (a board filled for a test)
    // seeds from their place, gathered in colonies of up to four touching blocks of a type, as pieces would be
    auto known = [&](int x, int y) { return blockSeeds_[y][x] > 0.f && seedType_[y][x] == g.cell(x, y).type + 1; };
    float stray[Game::H][Game::W];
    int strayPlace[Game::H][Game::W] = {};
    for (auto& row : stray)
        for (float& v : row) v = -1.f;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const int type = g.cell(x, y).type;
            if (type < 0 || known(x, y) || stray[y][x] >= 0.f) continue;
            const float co = 0.02f + 0.96f * std::fmod(std::sin(x * 39.346f + y * 11.135f) * 24634.6345f + 1e5f, 1.f);
            int qx[4] = {x}, qy[4] = {y}, n = 1;
            stray[y][x] = co;
            for (int k = 0; k < n && n < 4; k++)
                for (int dir = 0; dir < 4 && n < 4; dir++) {
                    const int xx = qx[k] + (dir == 0) - (dir == 1), yy = qy[k] + (dir == 2) - (dir == 3);
                    if (xx < 0 || xx >= Game::W || yy < Game::HIDDEN || yy >= Game::H || g.cell(xx, yy).type != type ||
                        known(xx, yy) || stray[yy][xx] >= 0.f || g.rowOffset(yy) != g.rowOffset(y))
                        continue;
                    stray[yy][xx] = co;
                    qx[n] = xx; qy[n] = yy; n++;
                }
            int minx = 99, maxy = -99;
            for (int k = 0; k < n; k++) { minx = std::min(minx, qx[k]); maxy = std::max(maxy, qy[k]); }
            for (int k = 0; k < n; k++) strayPlace[qy[k]][qx[k]] = (qx[k] - minx) + 8 * (maxy - qy[k]);
        }
    auto colonyAt = [&](int x, int y) { return known(x, y) ? colonies_[y][x] : stray[y][x]; };
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            const float sd = known(x, y) ? blockSeeds_[y][x] : 0.02f + 0.96f * std::fmod(std::sin(x * 12.9898f + y * 78.233f) * 43758.5453f + 1e5f, 1.f);
            const float co = colonyAt(x, y);
            // (the blocks of its colony touching it; a row still closing up after a clear touches none)
            auto with = [&](int xx, int yy) {
                return xx >= 0 && xx < Game::W && yy >= Game::HIDDEN && yy < Game::H && g.cell(xx, yy).type == c.type &&
                       g.rowOffset(yy) == g.rowOffset(y) && colonyAt(xx, yy) == co;
            };
            const int nb = (with(x, y - 1) ? 1 : 0) | (with(x, y + 1) ? 2 : 0) | (with(x - 1, y) ? 4 : 0) | (with(x + 1, y) ? 8 : 0);
            cell(p.x, p.y, 0.5f, tintOf(c.type) * darken, 1.f, 0, 0.f, c.type, sd, co, nb, known(x, y) ? places_[y][x] : strayPlace[y][x]);
        }
    for (const Fading& f : fading_)
        cell(f.x, f.y, 0.5f, tintOf(f.type), 1.f, 0, saturate(f.t / CLEAR_T), f.type, f.seed, f.colony, f.nb, f.place);
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            for (int i = 0; i < 4; i++) {
                const auto& c = gc[i];
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cell(w.x, w.y, 0.5f, tintOf(p.type), 0.9f, 1, 0.f, p.type, pieceSeeds_[i], pieceColony_, pieceMask(gc, i), piecePlace(gc, i));
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            const auto& c = cells[i];
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, tintOf(p.type), 1.f, 0, 0.f, p.type, pieceSeeds_[i], pieceColony_, pieceMask(cells, i), piecePlace(cells, i));
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
        for (int i = 0; i < 4; i++) {
            const auto& c = cells[i];
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            cell(px, py, 0.5f * sc, tintOf(type), alpha, 0, 0.f, type, 0.1f + 0.11f * type + 0.2f * i, 0.3f + 0.09f * type,
                 pieceMask(cells, i), piecePlace(cells, i));
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void CoralReefUniverse::sprite(vec2 p, float size, const vec3& col, float alpha) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, 0.f}, {col.x, col.y, col.z, alpha}});
}

// =====================================================================================================
// The HUD
// =====================================================================================================

void CoralReefUniverse::drawHud(const std::vector<HudRect>& panels) {
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

// The lettering, by the host (text.hpp): Nunito capitals, widely spaced, for the labels and the numbers; Fraunces'
// soft italics for the song, the variant, the notices, and the reef's stage after the universe's name; each line on
// a soft halo of the water's veil. The inks are the scene's (linear), toned as the post tones the scene: dimmed,
// their highlights rolled off, to sRGB.
void CoralReefUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* sans = text::font(FONT_SANS);
    text::Font* ital = text::font(FONT_ITAL);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount), dim = 1.f - dimT_ * 0.5f;
    auto tone = [&](const vec3& lin) {
        vec3 c = lin * dim;
        for (float* v : {&c.x, &c.y, &c.z}) *v /= 1.f + std::max(*v - 0.8f, 0.f) * 0.8f;
        return ugl::toSrgb(c);
    };
    auto inkOf = [&](const vec3& c) { return tone(lerp(c, bridge_.ink, b)); };
    const vec3 ink = inkOf(pal_.ink), accent = inkOf(pal_.accent);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    float halo = 0.75f; // the halo's amount
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        // the name in spaced capitals, then the variant and the reef's stage in italics, a little higher
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        size_t n = 1;
        while (n < var.size() && ((unsigned char)var[n] & 0xC0) == 0x80) n++;
        if (!var.empty()) var = var.substr(0, n) + text::lower(var.substr(n));
        const float sz = base * 0.95f, tr = sz * 0.28f, vsz = base * 1.3f, gap = base * 0.75f;
        const float a = saturate(0.35f + h.alpha * 1.6f);
        // (each run was placed by its own cap line; a span's spacing stays after its last letter, the gaps take it
        // back)
        const float dy = -sz * 0.22f + text::capHeight(ital) * vsz - text::capHeight(sans) * sz;
        put(sans, name, sz, tr, ink, a);
        if (!var.empty()) {
            text::Span& v = out.add(var, ital, vsz, accent, 0.f, a * 0.9f);
            v.gap = (gap - tr) / vsz;
            v.dy = dy;
        }
        text::Span& st = out.add(stageName(std::max(0.f, shownLines_)), ital, vsz, ink, 0.f, a * 0.72f);
        st.gap = (gap * 1.3f - (var.empty() ? tr : 0.f)) / vsz;
        st.dy = dy;
        break;
    }
    case HUD_SECTION: put(sans, h.text, base * 0.9f, base * 0.45f, ink, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(ital, h.text, base * 1.65f, 0.f, ink, h.alpha * 1.3f, -base * 0.35f); break;
    case HUD_INFO: put(sans, h.text, base * 0.66f, base * 0.12f, ink, saturate(alpha * 1.3f) * 0.8f); break;
    case HUD_LABEL: put(sans, h.text, base * 0.78f, base * 0.42f, inkOf(lerp(pal_.ink, pal_.accent, 0.5f)), saturate(alpha * 1.2f) * 0.92f); break;
    case HUD_VALUE: put(sans, h.text, base * 1.05f, base * 0.04f, ink, alpha); break;
    case HUD_NOTICE: put(ital, h.text, base * 1.55f, 0.f, accent, alpha); break;
    case HUD_KEY: put(sans, h.text, base * 0.75f, base * 0.06f, ink, alpha); break;
    case HUD_TOAST: put(sans, h.text, base * 1.3f, base * 0.45f, ink, alpha, -base * 0.2f); halo = 0.85f; break;
    case HUD_TOAST_SUB: put(ital, h.text, base * 1.4f, 0.f, accent, alpha * 0.9f); halo = 0.85f; break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * height_ / 1080.f) put(ital, h.text, base * 1.3f, 0.f, ink, alpha, -base * 0.1f);
        else put(sans, h.text, base * 0.85f, base * 0.14f, h.accent ? accent : ink, alpha);
        halo = 0.85f;
        break;
    default: put(sans, h.text, base * 0.85f, base * 0.12f, h.accent ? accent : ink, alpha); break;
    }
    // the letters as the scene drew them, on a halo of the veil
    text::Lettering& l = lettering_[halo > 0.8f ? 1 : 0];
    l.glsl = shaders::LETTER_GLSL;
    l.p0 = vec4(tone(lerp(pal_.veil * 0.8f, bridge_.color, b)), halo);
    out.lettering = &l;
    out.alpha = saturate(fade_);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void CoralReefUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    // REFALL_GPUTIME: the GPU time of a frame, printed every 120 frames
    static const bool gpuTime = std::getenv("REFALL_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    static int gpuN = 0;
    static double gpuSum = 0.0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);

    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 3.f) : 0.f, overT_ >= 0.f ? 3.f : 0.8f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, slow_);
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
        // the drop's downbeat: every coral releases its eggs at once; the mantas come in on one side, the
        // schools gather on the other
        dropT_ = 0.f;
        mantaT_ = 0.f;
        mantaSide_ = -mantaSide_;
        dropOn_ = true;
        for (const Slot& sl : slots_) {
            if (sl.grow < 0.5f) continue;
            const int n = 5 + (int)(sl.size * 4.f);
            for (int i = 0; i < n; i++) spawnEgg(sl, -rng_.range(0.f, 1.2f));
        }
        if (std::getenv("REFALL_GPUTIME")) std::fprintf(stderr, "[coralreef] the drop at %.1f s: %d eggs\n", time_, (int)eggs_.size());
    }
    if (!isDrop && wasDrop_) dropOn_ = false;
    wasDrop_ = isDrop;
    dropT_ += dt;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 0.8f : 0.3f, dt);
    const float life = smoothstepf(0.f, 1.f, dropAmt_);
    dropLight_ = approach(dropLight_, isDrop ? 1.f : 0.f, isDrop ? 0.45f : 0.16f, dt);
    swell_ = approach(swell_, 0.f, 0.4f, dt);

    // ---- The lines -> the reef.
    if (reefSeed_ == 0) { reefSeed_ = seed_ ? seed_ : 1; reefDirty_ = true; }
    if (reefDirty_) {
        // (a rebuild keeps what has grown: the same seed lays the same reef)
        std::vector<float> grown;
        for (const Slot& s : slots_) grown.push_back(s.grow);
        buildReef();
        for (size_t i = 0; i < slots_.size() && i < grown.size(); i++) {
            slots_[i].grow = grown[i];
            if (grown[i] > 0.f) slots_[i].startT = time_ - GROW_T * grown[i];
        }
        if (schools_.empty()) buildSchools();
    }
    {
        const float lines = f.bare ? lines_ : paceLines(game, (float)extraLines(time_));
        const bool first = shownLines_ < 0.f;
        if (!first && lines < lines_ - 0.01f && !game.over()) {
            // a new game: the reef recedes and grows again from the bare rock
            for (Slot& s : slots_) { s.startT = -1.f; s.awaiting = false; }
            seeds_.clear();
        }
        const float before = lines_;
        lines_ = lines;
        if (first) {
            shownLines_ = lines;
            shownLinesFirst_ = true;
            for (Slot& s : slots_)
                if (s.birth <= lines) { s.grow = 1.f; s.startT = time_ - GROW_T; }
        } else shownLinesFirst_ = false;
        // the new slots: their seeds drift out of the cleared rows and settle; without a clear (a test hook, a
        // new song's pace), they simply grow, one after the other
        int fresh = 0;
        for (int i = 0; i < (int)slots_.size(); i++) {
            Slot& s = slots_[i];
            if (s.startT >= 0.f || s.awaiting || s.birth > lines_) continue;
            if (clearPending_ && !clearPts_.empty() && s.birth > before - 0.01f && fresh < 8) {
                Seed sd;
                const vec2 bp = clearPts_[rng_.irange(0, (int)clearPts_.size() - 1)];
                sd.from = boardToWorld(bp, camD_ * rng_.range(0.7f, 0.9f));
                sd.to = s.p + vec3(0.f, 0.25f * s.size, 0.f);
                sd.ctl = lerp(sd.from, sd.to, 0.4f) + vec3(0.f, camD_ * rng_.range(0.13f, 0.22f), 0.f);
                sd.age = -0.25f - 0.35f * fresh;
                sd.life = rng_.range(2.6f, 3.4f);
                sd.seed = rng_.uniform();
                sd.slot = i;
                sd.col = lerp(pal_.reef[s.colIdx[1]], vec3(1.f), 0.15f);
                seeds_.push_back(sd);
                s.awaiting = true;
            } else {
                s.startT = time_ + std::min(0.4f * fresh, 1.5f) + (fresh > 4 ? 0.05f * (fresh % 40) : 0.f);
            }
            fresh++;
        }
        if (clearPending_ && std::getenv("REFALL_GPUTIME")) std::fprintf(stderr, "[coralreef] a clear: %d new corals (lines %.2f -> %.2f)\n", fresh, before, lines_);
        clearPending_ = false;
        clearPts_.clear();
        // the growth shown (the schools, the crusts): springs after the lines, its speed capped
        const float cap = 1.5f * std::max(1.f, paceScale());
        shownLines_ += std::clamp((lines_ - shownLines_) * 1.2f * wdt, -8.f * wdt, cap * wdt);
        if (lines_ < shownLines_ - 5.f) shownLines_ = approach(shownLines_, lines_, 6.f, dt);
        // the camera follows the growth on a calm spring (a new game dives back down to the polyp)
        // (the setup: it dives down onto its view, the light coming down from the surface; the teardown: it drifts
        // back, the light sinking to the deep)
        if (camL_ < 0.f) { camL_ = shownLines_; camLv_ = 0.f; }
        const float w = 1.3f;
        camLv_ += (w * w * (shownLines_ - camL_) - 2.f * w * camLv_) * dt;
        camL_ += camLv_ * dt;
    }
    {
        const float in = 1.f - smoothstepf(0.f, 0.7f, f.setup), out = smoothstepf(0.2f, 1.f, f.teardown);
        dive_ = in * in;
        drift_ = out * out;
    }
    updateCamera();
    for (Rock& b : rocks_)
        if (b.birth >= 0.f) b.grow = approach(b.grow, shownLines_ >= b.birth ? 1.f : 0.f, 0.3f, wdt);
    for (Slot& s : slots_) {
        float target = 0.f;
        if (s.startT >= 0.f && time_ >= s.startT) {
            const float g = saturate((time_ - s.startT) / GROW_T);
            target = g * g * (3.f - 2.f * g);
            s.grow = target;
        } else if (s.startT < 0.f) s.grow = approach(s.grow, 0.f, 0.35f, wdt);
    }

    // ---- The palette: the variant, slow motion's deep blue, game over's dusk, the drop's swell.
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
        // the drop: a magical dusky blue-violet, so the spawn glows
        const float dusk = smoothstepf(0.f, 1.f, dropLight_) * (1.f - slow_);
        if (dusk > 0.01f) {
            Palette d = pal_;
            d.water = lin(0x2c3aa8); d.deep = lin(0x0e0c48); d.haze = lin(0x7a68d8); d.surface = lin(0x9a8af0);
            d.sun = pal_.sun * vec3(0.8f, 0.72f, 1.f); d.shade = lin(0x2a2878); d.rim = lin(0xb08aff);
            d.caustic = lin(0xd0b8ff); d.ray = lin(0xc4a8ff);
            d.sand = pal_.sand * vec3(0.8f, 0.78f, 0.95f); d.sand2 = pal_.sand2 * vec3(0.8f, 0.78f, 0.95f);
            d.rays = pal_.rays * 1.15f; d.caustics = pal_.caustics * 0.8f; d.glow = std::max(pal_.glow, 0.35f);
            lerpPalette(pal_, d, dusk * 0.8f);
            for (int i = 0; i < 7; i++) pal_.piece[i] = basePal_.piece[i];
            pal_.ink = basePal_.ink; pal_.accent = basePal_.accent; pal_.veil = basePal_.veil;
        }
        const float deep = std::max({slow_, 0.7f * over_, 1.f - smoothstepf(0.05f, 0.65f, f.setup), smoothstepf(0.3f, 0.95f, f.teardown)});
        if (deep > 0.01f) {
            Palette d = pal_;
            d.water = lin(0x0c2c8c); d.deep = lin(0x030c3a); d.haze = lin(0x2a52c8); d.surface = lin(0x4a7ae8);
            d.sun = pal_.sun * vec3(0.45f, 0.6f, 1.f); d.shade = lin(0x0c1650); d.rim = lin(0x4a7aff);
            d.caustic = lin(0x7aa8ff); d.ray = lin(0x6a9aff);
            d.rays = pal_.rays * 0.8f; d.caustics = pal_.caustics * 0.6f; d.sat = 1.05f;
            lerpPalette(pal_, d, deep);
            // (the pieces and the HUD keep their colors)
            for (int i = 0; i < 7; i++) pal_.piece[i] = basePal_.piece[i];
            pal_.ink = basePal_.ink; pal_.accent = basePal_.accent;
        }
    }

    // ---- The music: the melody's height, the energy, the kicks on the caustics.
    {
        float sw = 0.f, sc = 0.f;
        for (int b = 3; b < 13; b++) { sw += music.bandsFast[b]; sc += music.bandsFast[b] * (float)b; }
        const float centroid = sw > 1e-4f ? sc / sw : 7.f;
        const float m = saturate((centroid - 5.5f) / 4.5f);
        melody_ = approach(melody_, music.hasSong && sw > 0.02f ? m : 0.5f, 1.2f, dt);
        energyS_ = approach(energyS_, saturate(0.25f + 0.35f * wm + 0.4f * wp + 0.2f * life), 0.6f, dt);
    }
    const float kick = std::max(music.kick, kickIn_) * (0.35f * wm + 0.65f * wp);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    kickK_ = std::max(approach(kickK_, 0.f, 2.5f, dt), kick);
    causticPhase_ += wdt * (0.55f + 0.15f * wm + 0.25f * wp) + kick * dt * 1.2f;
    surge_ += wdt * (0.6f + 0.25f * wm + 0.35f * wp);

    // ---- Fish.
    if (swirlT_ >= 0.f) swirlT_ += dt;
    float swirlK = swirlT_ >= 0.f ? smoothstepf(0.f, 1.5f, swirlT_) * (1.f - smoothstepf(6.f, 9.f, swirlT_)) : 0.f;
    if (swirlT_ > 9.f) swirlT_ = -1.f;
    {
        // the drop: every school gathers into one great bait ball on the other side, disperses when it ends
        const float ball = smoothstepf(0.f, 1.f, mantaK_);
        if (ball > swirlK) { swirlK = ball; swirlC_ = vec3(-mantaSide_ * 10.5f, 3.4f, 1.f); }
    }
    swirl_ = swirlK;
    if (!f.paused) updateFish(wdt, energyS_, melody_, swirlK);

    // ---- The drop: the mantas' loop, the eggs (a burst on the downbeat, then the corals keep spawning while the
    // drop lasts; at its end they thin out over several seconds), the corals breathing with the beat.
    const float enter = smoothstepf(0.f, 1.f, mantaK_);
    mantaK_ = approach(mantaK_, dropOn_ ? 1.f : 0.f, dropOn_ ? 0.18f : 0.12f, dt);
    mantaT_ += wdt;
    vec3 mantaP[2], mantaD[2];
    float mantaBank[2] = {0.f, 0.f};
    for (int k = 0; k < 2; k++) {
        const float t = mantaT_ - 1.6f * k;
        const vec3 p0 = mantaAt(t, enter), p1 = mantaAt(t + 0.15f, enter), p2 = mantaAt(t + 0.3f, enter);
        mantaP[k] = p0 + (camR_ * (k == 0 ? 0.f : -1.8f * mantaSide_) + vec3(0.f, k == 0 ? 0.f : -1.1f, 0.f)) * std::min(camD_ / 15.6f, 6.f);
        mantaD[k] = normalize(p1 - p0);
        const vec3 d2 = normalize(p2 - p1);
        mantaBank[k] = std::clamp(-cross(mantaD[k], d2).y * 25.f, -0.6f, 0.6f) + 0.3f * (mantaD[k].x >= 0.f ? 1.f : -1.f);
    }
    const bool mantaOn = mantaK_ > 0.01f && overT_ < 0.f && camD_ > 7.f;
    if (!f.paused) {
        if (dropOn_) {
            spawnAcc_ += dt * (40.f + 0.5f * (float)slots_.size());
            spawnAcc_ = std::min(spawnAcc_, 40.f);
            while (spawnAcc_ >= 1.f && !slots_.empty()) {
                spawnAcc_ -= 1.f;
                const Slot& sl = slots_[rng_.irange(0, (int)slots_.size() - 1)];
                if (camL_ > 36.f) {
                    // (at the tower's scale: a pink haze of eggs rising from the colonies all along its flanks)
                    const Slot& big = slots_[rng_.irange((int)slots_.size() / 2, (int)slots_.size() - 1)];
                    if (big.grow < 0.5f && sl.grow < 0.5f) continue;
                    Slot tmp = big.grow >= 0.5f ? big : sl;
                    tmp.size = 0.02f * camD_;
                    spawnEgg(tmp, 0.f);
                } else if (camD_ < 5.f || camD_ > 40.f) {
                    // (at the polyp's or the bommie's scale: eggs drifting up through the view)
                    Slot tmp = sl;
                    tmp.p = anchor_ + vec3(rng_.range(-1.6f, 1.6f), rng_.range(-0.4f, 0.2f), rng_.range(-1.2f, 0.6f)) * camD_;
                    tmp.p.y = std::max(tmp.p.y, reef::groundH(tmp.p.x, tmp.p.z, mounds_, moundCount_));
                    tmp.size = 0.1f * camD_;
                    spawnEgg(tmp, 0.f);
                } else if (sl.grow > 0.5f) spawnEgg(sl, 0.f);
                else {
                    // (and from the water itself, so a young reef's drop is full of them too)
                    Slot tmp = sl;
                    tmp.p = vec3(rng_.range(-24.f, 24.f), rng_.range(-0.5f, 3.f), rng_.range(-20.f, 6.f));
                    tmp.size = 1.f;
                    spawnEgg(tmp, 0.f);
                }
            }
        }
        for (Egg& e : eggs_) {
            e.age += wdt;
            if (e.age < 0.f) continue;
            const float es = e.r / 0.08f;
            const float rise = (0.28f + 0.18f * e.seed) * es;
            e.p += vec3(0.18f * std::sin(time_ * 0.7f + e.seed * 40.f) * es, rise, 0.12f * std::cos(time_ * 0.6f + e.seed * 30.f) * es) * wdt;
            if (!dropOn_) e.fade = approach(e.fade, 0.f, 0.15f + 0.2f * e.seed, wdt);
            if (e.p.y > surfY_ - 0.2f) e.life = std::min(e.life, e.age);
            if (camL_ <= 36.f && e.r > 0.4f) e.fade = approach(e.fade, 0.f, 0.3f, wdt); // (a new game: back to the reef)
        }
        eggs_.erase(std::remove_if(eggs_.begin(), eggs_.end(), [](const Egg& e) { return e.age >= e.life || e.fade <= 0.f; }), eggs_.end());
    }
    {
        // the corals' breath: on the beat, every two beats (the host's pulse), eased
        const float bp = music.hasSong ? std::max(music.beatPulse, 0.6f * music.kick) : 0.f;
        pulse_ = std::max(approach(pulse_, 0.f, 1.6f, dt), bp);
    }
    const float coralPulse = smoothstepf(0.f, 1.f, pulse_) * dropLight_;

    // ---- Particles in the world.
    for (Bubble& b : bubbles_) {
        b.age += wdt;
        if (b.age < 0.f) continue;
        const float drag = std::exp(-1.1f * wdt);
        b.v.x *= drag;
        b.v.z *= drag;
        const float bs = std::clamp(b.r / 0.08f, 0.01f, 3.f);
        b.v.y = std::min(b.v.y + 0.35f * bs * wdt, 1.6f * bs);
        b.p += (b.v + vec3(0.12f * std::sin(time_ * 3.f + b.seed * 30.f), 0.f, 0.08f * std::cos(time_ * 2.5f + b.seed * 20.f)) * bs) * wdt;
        b.r *= 1.f + 0.03f * wdt;
        if (b.p.y > surfY_ - 0.1f) b.life = std::min(b.life, b.age);
    }
    bubbles_.erase(std::remove_if(bubbles_.begin(), bubbles_.end(), [](const Bubble& b) { return b.age >= b.life; }), bubbles_.end());
    for (Seed& s : seeds_) {
        s.age += wdt;
        if (s.age >= s.life && s.slot >= 0 && s.slot < (int)slots_.size()) {
            Slot& sl = slots_[s.slot];
            if (sl.awaiting) { sl.awaiting = false; sl.startT = time_; }
        }
    }
    seeds_.erase(std::remove_if(seeds_.begin(), seeds_.end(), [](const Seed& s) { return s.age >= s.life; }), seeds_.end());
    for (auto& fd : fading_) fd.t += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > CLEAR_T; }), fading_.end());
    for (Puff& p : puffs_) {
        p.age += dt;
        if (p.age < 0.f) continue;
        p.p = V(p.p.x + p.v.x * dt, p.p.y + p.v.y * dt);
        p.v = V(p.v.x * std::exp(-3.f * dt), p.v.y + 0.8f * dt);
    }
    puffs_.erase(std::remove_if(puffs_.begin(), puffs_.end(), [](const Puff& p) { return p.age >= p.life; }), puffs_.end());

    cells_.clear();
    sprites_.clear();
    if (!f.bare) buildBoard(game);

    // =========================================================== the world (multisampled, at the full resolution)
    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const vec3 sunDir = normalize(vec3(std::sin(pal_.sunAz) * std::cos(pal_.sunEl), std::sin(pal_.sunEl),
                                       std::cos(pal_.sunAz) * std::cos(pal_.sunEl) * -0.6f));
    const float lightK = (1.f + 0.08f * wm + 0.15f * wp + 0.12f * life + 0.08f * swell_) * (1.f + 0.2f * smoothstepf(30.f, 37.f, camL_));
    auto setReef = [&](GLuint p) {
        set3f(p, "uEye", eye_);
        set3f(p, "uSunDir", sunDir);
        set3f(p, "uSunC", pal_.sun * (1.08f * lightK));
        set3f(p, "uShadeC", pal_.shade * 1.1f);
        set3f(p, "uRimC", pal_.rim);
        set3f(p, "uWaterC", pal_.water);
        set3f(p, "uDeepC", pal_.deep);
        set3f(p, "uHazeC", pal_.haze);
        set3f(p, "uCausticC", pal_.caustic * (0.9f + 0.25f * wp));
        // (the water's clarity grows with the scale: the reef's whole size reads through its haze)
        set1f(p, "uFogD", pal_.fog * (1.f + 0.06f * wp) * std::pow(std::max(1.f, camD_ / 15.6f), 1.f));
        set1f(p, "uTime", time_);
        set1f(p, "uCaustK", pal_.caustics * (1.f + 0.25f * kickK_));
        set1f(p, "uCaustPhase", causticPhase_);
        set1f(p, "uBridge", bridge);
        set1f(p, "uNight", pal_.night);
        set1f(p, "uGlowK", pal_.glow * (1.f + 0.3f * wp + 0.3f * life));
        set3f(p, "uBridgeCol", bridge_.color);
        set1f(p, "uLensD", 0.04f * camD_);
        set1f(p, "uSurfY", surfY_);
        set1f(p, "uTowerH", towerH_);
        float mv[reef::MAX_MOUNDS * 4], ms[reef::MAX_MOUNDS];
        for (int i = 0; i < reef::MAX_MOUNDS; i++) {
            const reef::Mound& m = mounds_[std::min(i, std::max(0, moundCount_ - 1))];
            mv[i * 4] = m.x; mv[i * 4 + 1] = m.z; mv[i * 4 + 2] = m.r; mv[i * 4 + 3] = m.h;
            ms[i] = m.seed;
        }
        glUniform4fv(ugl::U(p, "uMounds"), reef::MAX_MOUNDS, mv);
        glUniform1fv(ugl::U(p, "uMoundSeed"), reef::MAX_MOUNDS, ms);
        glUniform1i(ugl::U(p, "uMoundN"), moundCount_);
    };

    msT_.bind();
    glClearColor(0, 0, 0, 1000.f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    // the water, the surface
    glUseProgram(progWater_);
    setReef(progWater_);
    set3f(progWater_, "uLook", look_);
    set3f(progWater_, "uCamR", camR_);
    set3f(progWater_, "uCamU", camU_);
    set2f(progWater_, "uTan", tanH_ * aspect_, tanH_);
    set3f(progWater_, "uSurfC", pal_.surface);
    ugl::drawFullscreen();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    // the seabed
    glUseProgram(progGround_);
    setReef(progGround_);
    setMat(progGround_, "uVP", vp3_);
    set3f(progGround_, "uSandC", pal_.sand);
    set3f(progGround_, "uSand2C", pal_.sand2);
    set3f(progGround_, "uRockC", pal_.rock);
    set3f(progGround_, "uRock2C", pal_.rock2);
    set3f(progGround_, "uCrustC", pal_.crust);
    set3f(progGround_, "uCrust2C", pal_.crust2);
    set1f(progGround_, "uCover", smoothstepf(9.f, 40.f, shownLines_));
    set1f(progGround_, "uSpread", 0.f);
    {
        // the place it began glows softly from afar
        const float mk = smoothstepf(38.f, 46.f, camL_);
        set4f(progGround_, "uMark", P0_.x, P0_.z, 2.f + 0.045f * camD_, mk * (1.1f + 0.4f * pal_.night));
        set3f(progGround_, "uMarkC", lerp(pal_.accent, vec3(1.f, 0.85f, 0.7f), 0.4f));
    }
    set3f(progGround_, "uReefA", pal_.reef[0]);
    set3f(progGround_, "uReefB", pal_.reef[2]);
    set3f(progGround_, "uReefC", pal_.reef[4]);
    set3f(progGround_, "uReefD", pal_.reef[1]);
    {
        // the mesh's centre, between the eye and what it looks at (snapped to the scale's own steps)
        const float step = std::exp2(std::round(std::log2(std::max(camD_ * 0.01f, 1e-4f))));
        const float cx = std::round(lerpf(eye_.x, anchor_.x, 0.6f) / step) * step, cz = std::round(lerpf(eye_.z, anchor_.z, 0.6f) / step) * step;
        set4f(progGround_, "uGrid", cx, cz, camD_ * 0.006f, std::clamp(camD_ * 3000.f, 5000.f, 4.0e6f));
    }
    glBindVertexArray(groundVao_);
    glDrawElements(GL_TRIANGLES, groundCount_, GL_UNSIGNED_INT, nullptr);
    const float pixK = 2.f * tanH_ / (float)lowH_ * 1.6f; // (what is under a couple of pixels is not drawn)
    auto tiny = [&](const vec3& p, float size) { return size < length(p - eye_) * pixK; };
    // the corals
    {
        std::vector<float> inst[reef::K_COUNT][reef::TEMPLATES];
        for (const Slot& s : slots_) {
            if (s.grow <= 0.002f || tiny(s.p, s.size)) continue;
            const float mature = 0.82f + 0.18f * smoothstepf(s.birth, s.birth + 18.f, shownLines_);
            const vec3 a = pal_.reef[s.colIdx[0]], b = pal_.reef[s.colIdx[1]];
            const float q[16] = {s.p.x, s.p.y, s.p.z, s.size * mature, s.yaw, s.grow, s.seed, (float)s.kind,
                                 a.x, a.y, a.z, KINDS[s.kind].sway, b.x, b.y, b.z, 1.f};
            inst[s.kind][s.tmpl].insert(inst[s.kind][s.tmpl].end(), q, q + 16);
        }
        glUseProgram(progCoral_);
        setReef(progCoral_);
        setMat(progCoral_, "uVP", vp3_);
        set3f(progCoral_, "uRockC", pal_.rock);
        set3f(progCoral_, "uRock2C", pal_.rock2);
        set3f(progCoral_, "uCrustC", pal_.crust);
        set3f(progCoral_, "uCrust2C", pal_.crust2);
        set1f(progCoral_, "uCover", smoothstepf(9.f, 40.f, shownLines_));
        {
            std::vector<float> rinst[reef::TEMPLATES];
            for (const Rock& b : rocks_) {
                if (b.grow <= 0.002f || tiny(b.p, b.size)) continue;
                const float q[16] = {b.p.x, b.p.y - (1.f - b.grow) * 0.3f * b.size, b.p.z, b.size, b.yaw, b.grow, b.seed, 7.f, 0, 0, 0, 0.f, 0, 0, 0, 0.f};
                rinst[b.tmpl].insert(rinst[b.tmpl].end(), q, q + 16);
            }
            for (int t = 0; t < reef::TEMPLATES; t++) {
                if (rinst[t].empty()) continue;
                const MeshGL& m = rockM_[t];
                glBindVertexArray(m.vao);
                glBindBuffer(GL_ARRAY_BUFFER, m.inst);
                glBufferData(GL_ARRAY_BUFFER, rinst[t].size() * sizeof(float), rinst[t].data(), GL_STREAM_DRAW);
                glDrawElementsInstanced(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr, (GLsizei)(rinst[t].size() / 16));
            }
        }
        set1f(progCoral_, "uPulse", coralPulse);
        const float sa = 0.18f + 0.1f * wm + 0.12f * wp;
        set2f(progCoral_, "uSurge", sa * std::cos(0.4f), sa * std::sin(0.4f));
        for (int k = 0; k < reef::K_COUNT; k++)
            for (int t = 0; t < reef::TEMPLATES; t++) {
                const std::vector<float>& v = inst[k][t];
                if (v.empty()) continue;
                const MeshGL& m = coral_[k][t];
                glBindVertexArray(m.vao);
                glBindBuffer(GL_ARRAY_BUFFER, m.inst);
                glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
                glDrawElementsInstanced(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr, (GLsizei)(v.size() / 16));
            }
    }
    // the fish, the manta
    {
        std::vector<float> inst[reef::F_COUNT];
        for (const Fish& fi : fish_) {
            if (tiny(fi.p, fi.size)) continue;
            const int sp = fi.school == -1 ? reef::F_CLOWN : fi.school == -2 ? (fi.home == -2 ? reef::F_ANTHIAS : reef::F_CHROMIS)
                                                                              : schools_[fi.school].species;
            const float q[12] = {fi.p.x, fi.p.y, fi.p.z, fi.size, fi.dir.x, fi.dir.y, fi.dir.z, fi.phase,
                                 (float)sp, 0.35f + 0.25f * saturate(fi.speed / 2.f), fi.bank, 1.f};
            inst[sp].insert(inst[sp].end(), q, q + 12);
        }
        glUseProgram(progFish_);
        setReef(progFish_);
        setMat(progFish_, "uVP", vp3_);
        set1f(progFish_, "uFishGlow", pal_.night * 0.6f);
        for (int s = 0; s < reef::F_COUNT; s++) {
            if (inst[s].empty()) continue;
            const MeshGL& m = fishM_[s];
            glBindVertexArray(m.vao);
            glBindBuffer(GL_ARRAY_BUFFER, m.inst);
            glBufferData(GL_ARRAY_BUFFER, inst[s].size() * sizeof(float), inst[s].data(), GL_STREAM_DRAW);
            glDrawElementsInstanced(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr, (GLsizei)(inst[s].size() / 12));
        }
        if (mantaOn) {
            float q[24];
            for (int k = 0; k < 2; k++) {
                const float v[12] = {mantaP[k].x, mantaP[k].y, mantaP[k].z, (k == 0 ? 2.1f : 1.8f) * std::min(camD_ / 15.6f, 6.f), mantaD[k].x, mantaD[k].y, mantaD[k].z,
                                     time_ * TAU / 4.6f - 1.3f * k, 7.f, 0.f, mantaBank[k], 1.f};
                std::copy(v, v + 12, q + 12 * k);
            }
            glBindVertexArray(manta_.vao);
            glBindBuffer(GL_ARRAY_BUFFER, manta_.inst);
            glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STREAM_DRAW);
            glDrawElementsInstanced(GL_TRIANGLES, manta_.count, GL_UNSIGNED_INT, nullptr, 2);
        }
    }
    // the billboards: bubbles, seeds, specks (blended, the distance in alpha kept)
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    {
        pinst_.clear();
        auto glow = [&](const vec3& p, float size, const vec3& c, float a, int mode, float seed) {
            if (a <= 0.004f) return;
            const float q[12] = {p.x, p.y, p.z, size, c.x, c.y, c.z, a, (float)mode, seed, 0.f, 0.f};
            pinst_.insert(pinst_.end(), q, q + 12);
        };
        // specks drifting in the water around the camera
        {
            Rng sr(0x5BECull);
            const float sc = camD_ / 15.6f;
            const vec3 drift(time_ * 0.12f, std::sin(time_ * 0.05f) * 0.3f, time_ * 0.05f);
            for (int i = 0; i < 260; i++) {
                const vec3 base(sr.range(-22.f, 22.f), sr.range(-1.f, 7.f), sr.range(-20.f, 12.f));
                vec3 p = base + drift + vec3(0.3f * std::sin(time_ * 0.3f + i), 0.f, 0.f);
                p.x = std::fmod(p.x + 22.f + 440.f, 44.f) - 22.f;
                p.z = std::fmod(p.z + 20.f + 320.f, 32.f) - 20.f;
                p = anchor_ + (p - TARGET0) * sc;
                if (p.y > surfY_ - 0.3f) continue;
                const float sz = sr.range(0.012f, 0.03f) * sc;
                glow(p, sz, lerp(pal_.haze, vec3(1.f), 0.5f), 0.35f + 0.3f * wp, 2, sr.uniform());
            }
        }
        const float bigK = smoothstepf(33.f, 38.f, camL_);
        if (bigK > 0.f) {
            // schools as shimmering clouds over the reef, and one great school circling above the place it began
            const float S = camD_;
            const vec3 mark = P0_;
            for (int sc = 0; sc < 9; sc++) {
                Rng sr(0xA11ull + (uint64_t)sc * 7919ull);
                vec3 c;
                float R, H;
                int n;
                if (sc == 0) { c = mark; R = 0.09f * S + 4.f; H = 0.06f * S + 2.f; n = 700; }
                else {
                    // round the tower's flanks, at all heights
                    const float a = sr.range(0.f, TAU) + time_ * 0.006f * (sc % 2 ? 1.f : -1.f);
                    const float dep = towerH_ * sr.range(0.08f, 0.8f);
                    const float d = reef::towerR(dep, a) + S * sr.range(0.04f, 0.12f) + 6.f;
                    c = vec3(reef::TOWER_CX + std::cos(a) * d, -dep, reef::TOWER_CZ + std::sin(a) * d);
                    R = S * sr.range(0.025f, 0.05f) + 3.f; H = S * sr.range(0.01f, 0.03f) + 1.f; n = 200;
                }
                const vec3 colA = sc % 3 == 0 ? vec3(0.75f, 0.92f, 1.f) : sc % 3 == 1 ? lerp(pal_.reef[3], vec3(1.f), 0.3f) : lerp(pal_.reef[7], vec3(1.f), 0.4f);
                const float spin = (sc % 2 ? 1.f : -1.f) * (0.18f + 0.06f * sc / 9.f);
                for (int i = 0; i < n; i++) {
                    const float a = sr.range(0.f, TAU) + time_ * spin * (0.8f + 0.4f * sr.uniform());
                    const float rr = R * std::sqrt(sr.range(0.15f, 1.f));
                    const vec3 p = c + vec3(std::cos(a) * rr, H * (1.f + 0.35f * sr.range(-1.f, 1.f)) + 0.08f * R * std::sin(a * 2.f + i), std::sin(a) * rr);
                    // each fish catches the light as it turns: a shimmer running round the school
                    const float sh = 0.55f + 0.45f * std::sin(a * 3.f + time_ * 2.f + sr.uniform() * 6.f);
                    glow(p, std::max(0.25f, S * (sc == 0 ? 0.0032f : 0.0026f)), colA * (1.f + 0.8f * sh), bigK * (0.6f + 0.4f * sh), 2, sr.uniform());
                }
            }
        }
        for (const Bubble& b : bubbles_) {
            if (b.age < 0.f || f.bare) continue;
            const float a = smoothstepf(0.f, 0.2f, b.age) * (1.f - smoothstepf(surfY_ - 1.2f, surfY_ - 0.1f, b.p.y));
            glow(b.p, b.r, lerp(pal_.haze, vec3(1.f), 0.4f), a, 0, b.seed);
        }
        for (const Egg& e : eggs_) {
            if (e.age < 0.f) continue;
            const float a = smoothstepf(0.f, 0.6f, e.age) * (1.f - smoothstepf(e.life - 1.5f, e.life, e.age)) * e.fade *
                            (0.75f + 0.25f * std::sin(time_ * 2.f + e.seed * 50.f));
            glow(e.p, e.r * (1.f + 0.4f * dropLight_), e.col, a, 3, e.seed);
        }
        for (const Seed& s : seeds_) {
            if (s.age < 0.f || f.bare) continue;
            const float t = smoothstepf(0.f, 1.f, saturate(s.age / s.life));
            const vec3 a = lerp(s.from, s.ctl, t), b = lerp(s.ctl, s.to, t);
            const vec3 p = lerp(a, b, t) + vec3(0.15f * std::sin(time_ * 2.f + s.seed * 30.f), 0.f, 0.f) * (1.f - t);
            const float al = smoothstepf(0.f, 0.15f, s.age / s.life) * (1.f - 0.5f * smoothstepf(0.85f, 1.f, s.age / s.life));
            glow(p, 0.34f * camD_ / 15.6f, s.col, al, 1, s.seed);
        }
        if (!pinst_.empty()) {
            glUseProgram(progGlow_);
            setReef(progGlow_);
            setMat(progGlow_, "uVP", vp3_);
            set3f(progGlow_, "uCamR", camR_);
            set3f(progGlow_, "uCamU", camU_);
            set1f(progGlow_, "uMinPx", 2.f * tanH_ / lowH_ * 0.9f);
            glBindVertexArray(glowVao_);
            glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
            glBufferData(GL_ARRAY_BUFFER, pinst_.size() * sizeof(float), pinst_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(pinst_.size() / 12));
        }
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    msT_.resolve(worldT_);

    // =========================================================== the scene (full resolution)
    sceneT_.bind();
    glUseProgram(progComp_);
    setReef(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progComp_, "uWorld", 0);
    set3f(progComp_, "uLook", look_);
    set3f(progComp_, "uCamR", camR_);
    set3f(progComp_, "uCamU", camU_);
    set2f(progComp_, "uTan", tanH_ * aspect_, tanH_);
    set3f(progComp_, "uRayC", pal_.ray * (1.f - bridge));
    set2f(progComp_, "uSunVP", 0.5f + 0.35f * std::sin(pal_.sunAz) * std::cos(pal_.sunEl) * 2.f, 1.f + 1.6f * std::tan(pal_.sunEl) * 0.5f);
    set1f(progComp_, "uRays", pal_.rays * (0.75f + 0.15f * wm + 0.3f * wp + 0.3f * life + 0.15f * swell_));
    set1f(progComp_, "uSat", pal_.sat);
    set1f(progComp_, "uFocus", camD_);
    set1f(progComp_, "uDof", 9.f * (1.f - smoothstepf(0.35f, 4.f, camD_)) * worldT_.h / 720.f);
    set1f(progComp_, "uBlurPx", (float)worldT_.h / lowH_); // (the well's blur as wide as it was)
    set1f(progComp_, "uRayScale", std::clamp(camD_ / 15.6f, 0.01f, 8.f));
    {
        // the well's rectangle in uv (the world is blurred behind it)
        const float u0 = (-5.3f - (viewC_.x - viewH_.x)) / (2.f * viewH_.x), u1 = (5.3f - (viewC_.x - viewH_.x)) / (2.f * viewH_.x);
        const float v0 = (-10.3f - (viewC_.y - viewH_.y)) / (2.f * viewH_.y), v1 = (10.3f - (viewC_.y - viewH_.y)) / (2.f * viewH_.y);
        set4f(progComp_, "uWellUv", u0, v0, u1, v1);
        set1f(progComp_, "uBlur", f.bare ? 0.f : 1.f - bridge);
    }
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", lerp(pal_.veil, bridge_.color, bridge));
        set3f(progVeil_, "uRail", lerp(pal_.rail * 0.5f, bridge_.ink * 0.4f, bridge));
        set1f(progVeil_, "uWellA", 0.93f);
        set1f(progVeil_, "uColA", 0.5f * (1.f - bridge));
        set1f(progVeil_, "uRailA", 0.45f);
        set1f(progVeil_, "uPx", pxPerUnit_);
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], -5.f, layout::LEFT_COLUMN[3]);
        set4f(progVeil_, "uColR", 5.f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();

        // tiny bubbles under the pieces (a hard drop), then the pieces
        for (const Puff& p : puffs_) {
            if (p.age < 0.f) continue;
            const float t = p.age / p.life;
            sprite(p.p, p.size, lerp(pal_.haze, vec3(1.f), 0.6f), 0.7f * (1.f - smoothstepf(0.5f, 1.f, t)));
        }
        auto drawSprites = [&]() {
            if (sprites_.empty()) return;
            glUseProgram(progSprite_);
            setMat(progSprite_, "uVP", vp_);
            glBindVertexArray(spriteVao_);
            glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
            glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
            sprites_.clear();
        };
        drawSprites();
        if (!cells_.empty()) {
            glUseProgram(progCell_);
            setMat(progCell_, "uVP", vp_);
            set1f(progCell_, "uTime", time_);
            set1f(progCell_, "uShimmer", 1.f - pal_.night * 0.5f);
            set3f(progCell_, "uLightC", pal_.sun);
            set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
            set3f(progCell_, "uBridgeInk", bridge_.ink);
            // a contact shadow under the blocks (at night a soft glow around them), then the blocks
            set1f(progCell_, "uGlow", pal_.night);
            set3f(progCell_, "uHaloC", vec3(0.f, 0.004f, 0.02f));
            set1f(progCell_, "uHaloA", lerpf(0.75f, 0.32f, pal_.night));
            set2f(progCell_, "uHaloOff", lerpf(0.07f, 0.f, pal_.night), lerpf(-0.09f, 0.f, pal_.night));
            glBindVertexArray(cellVao_);
            glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
            glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
            for (int pass = 0; pass < 2; pass++) {
                set1f(progCell_, "uPass", (float)pass);
                glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
            }
        }
    }
    drawHud(f.panels);
    fade_ = f.fade;
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
        gpuSum += ns / 1e6;
        if (++gpuN % 120 == 0) {
            std::fprintf(stderr, "[coralreef] gpu %.2f ms/frame (%d corals, %d fish, %dx%d world)\n", gpuSum / 120.0,
                         (int)std::count_if(slots_.begin(), slots_.end(), [](const Slot& s) { return s.grow > 0.f; }), (int)fish_.size(),
                         worldT_.w, worldT_.h);
            gpuSum = 0.0;
        }
    }
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_coralreef() {
    static const UniverseEntry e{
        "CORAL REEF", "a sunny shallow reef that grows with every cleared line, from bare rock and white sand to corals full of fish",
        [] { return std::unique_ptr<Universe>(new CoralReefUniverse()); },
        // Fits most songs; the bright, flowing ones a little better.
        [](const SongLook& l) { return 0.85f + 0.2f * l.brightness + 0.1f * (1.f - l.density); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
