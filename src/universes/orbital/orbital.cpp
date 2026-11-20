// ORBITAL STATION: a space station builds itself in orbit behind the well, modules docking with every cleared line.
//
// It starts as a single crew capsule, its two little wings out, over the planet. Each cleared line flies the next
// modules in (two at first, more later), one after the other; for each, a tug carries it from far off and pushes it
// to a point off its seat, then it eases home (the robot arm on the first lab reaches out, grapples it and guides it
// in when it is within reach), the seat lights up as it latches, a few welding sparks fly, and the tug backs off and
// leaves. Over the song the station grows absurdly: nodes and labs, a cupola, the truss across with its solar wings
// turning to the sun, the habitat wheel behind the core (a hub, three pairs of spokes, six arcs; complete, it turns),
// a visiting ship, more truss and arrays, radiators, a dish; then a spindle down to a great hub and the great forest
// ring, 18 km across, its arcs brought in by squads of giant tugs, forests, meadows, a river, lakes and clouds
// inside, towns lighting up as it fills; then mirrors around its rim and liners mooring along it, and stages each an
// order of magnitude bigger (station.cpp) every four lines or so, up to the finale's crossed rings at ~50 lines, one
// song. The camera pulls back with it (a spring in log space): from ten meters around the capsule to some 400,000 km.
//
// The planet below (ray-traced at a reduced resolution): continents, oceans with the sun's glint, clouds and their
// shadows, the terminator, the cities' lights, the limb's glowing air, an aurora by night; the sun, the moon, the
// Milky Way and stars (the stars measured in screen pixels through the sky projection's Jacobian, as GIANT TREE does).
// The framing is a film's: the sky turns with the camera's heading, so each variant's sun keeps its place on screen
// (at dawn just over the limb left of the well, the lit crescent under it, the night with its cities across), seen
// through a lens (a starburst, a halo, ghosts, an anamorphic streak) and a light grade (warm highlights, deep blacks).
// The station is instanced solids (boxes and parametric cylinders, spheres, tori and bands) with their own
// materials (insulation blankets, gold foil, solar cells, radiators, lattice trusses, the ring's land), lit by the sun
// with its shadow map, by the planet's light from below and its reflection; lamps, windows and thrusters glow (4x
// multisampling, alpha to coverage for the lattices, a soft bloom).
//
// The song: calm is a quiet station (few lamps); mid lights more lamps, the nav lights blink every two beats and a
// few drones make their rounds; a peak lights everything, more drones, welding sparks on the beat grid, lamps lifted
// by the kicks. **The drop** (a real one only, not a chorus) switches every lamp on in a sequence from the core
// outward, spins the rings up and sends heads of light chasing round them on the beat (round the wheel every two
// bars, round the great ring every four); before the wheel exists, the drones gather into a halo round the station
// and chase their lights round it. Slow motion is an eclipse (the sunlight fades, the windows glow). Game over powers
// the station down from its rim inward; a new game takes the modules away (they shrink back) to the capsule.
//
// Around a song: the setup powers the station up from the core outward as the camera eases in; the teardown powers it
// down from the rim inward as the camera draws back.
//
// The pieces are the station's modules, flat and spare: flat gunmetal frames round each piece's outline, flat navy
// insets, and one circuit of thin cyan lines per piece running through its cells (its style by the piece's type, its
// hubs' markers touched by the piece's color; a piece keeps its id through falls and clears, see syncIds). Cleared
// rows flare and shrink into their light, which flies out to the module's seat.
//
// Layout contract: the board is drawn flat (orthographic at the reference camera's scale); only the world behind it
// has its own 3D camera.
#include "universes/orbital/orbital.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/orbital/builder.hpp"
#include "universes/orbital/shaders.hpp"
#include "universes/registry.hpp"

using namespace orbital;

namespace {

// Jura (Medium and Light), SIL Open Font License, see OFL-Jura.txt.
const unsigned char FONT_MED[] = {
#include "universes/orbital/Jura-Medium.ttf.inc"
};
const unsigned char FONT_LIGHT[] = {
#include "universes/orbital/Jura-Light.ttf.inc"
};

namespace shaders = orbital_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

std::string withHead(const char* src, const std::string& head) {
    std::string s(src);
    s.insert(s.find('\n') + 1, head);
    return s;
}
GLuint compile(const char* vs, const char* fs, const char* name) {
    return ugl::program(withHead(vs, shaders::COMMON_GLSL).c_str(), withHead(fs, shaders::COMMON_GLSL).c_str(), name);
}
GLuint compilePart(const char* vs, const char* fs, const char* name) {
    const std::string vh = std::string(shaders::COMMON_GLSL) + shaders::PART_HEAD;
    const std::string fh = std::string(shaders::COMMON_GLSL) + shaders::LATTICE_GLSL;
    return ugl::program(withHead(vs, vh).c_str(), withHead(fs, fh).c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 lerp2(vec2 a, vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
vec2 mul(vec2 a, float k) { return {a.x * k, a.y * k}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 L(unsigned v) { return ugl::toLinear(hex(v)); }

using Palette = OrbitalUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));
const char* VARIANT_NAMES[] = {"BLUE MARBLE", "ORBITAL DAWN", "AURORA NIGHT", "RED PLANET"};
constexpr int VARIANT_COUNT = 4;

// The variants: the planet, the hour, the station's paint and lamps, the seven pieces.
Palette paletteOf(int v) {
    Palette p{};
    v = ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT;
    // shared defaults
    p.hull = L(0xf0eee8); p.hull2 = L(0xd6dccb); p.gold = L(0xe6ae48); p.solar = L(0x1a2a6a); p.radiator = L(0xf2f6fa);
    p.dark = L(0x2a2c30); p.window = L(0xffc880); p.trim = L(0xc8ccd2); p.navR = L(0xff3a30); p.navG = L(0x30ff78);
    p.lamp = L(0xffe0b0); p.chase = L(0x60e0ff); p.thrust = L(0x80b0ff);
    p.landA = L(0x22603a); p.landB = L(0x3f8040); p.lake = L(0x1a6aa0); p.rock = L(0x6a6a64); p.town = L(0xffc070);
    p.ringCloud = L(0xffffff);
    p.space = L(0x02040c); p.milky = L(0x34446a); p.cloud = L(0xffffff); p.ice = L(0xf4f8ff);
    p.moonDir = normalize(vec3(-0.85f, 0.42f, -0.4f));
    p.pole = normalize(vec3(0.3f, 0.45f, -0.84f));
    p.ringDay = 0.5f;
    p.haze = 1.f;
    p.wrap = 0.f;
    p.moonSize = 0.f; p.sunVis = 1.f; p.sunSize = 0.011f; p.keyMoon = 0.f; p.planetType = 0.f; p.clouds = 1.f;
    p.cityK = 1.f; p.auroraK = 0.f; p.milkyK = 0.25f; p.bloom = 0.5f; p.nightLift = 0.f; p.stars = 0.7f; p.keyK = 1.f;
    p.airglow = L(0x40ff90) * 0.1f; p.aurora1 = L(0x30ff9a); p.aurora2 = L(0xd040ff);
    switch (v) {
    case 0: {
        // BLUE MARBLE: the day side under a high sun, blue oceans and white clouds, the terminator on the far horizon
        // (the sun seen low in the frame's upper left, its flare across; the planet lit by it high overhead)
        p.litDir = normalize(vec3(0.45f, 0.82f, 0.12f));
        p.sunDir = normalize(vec3(-0.878f, -0.053f, -0.474f));
        p.keyDir = p.litDir;
        p.sunC = vec3(1.f, 0.98f, 0.94f) * 2.6f; p.sunGlow = L(0xfff0d0); p.keyC = p.sunC;
        p.earthC = L(0x3a64a8) * 0.32f; p.amb = L(0x0a0e18) * 0.6f; p.envUp = L(0x05070c); p.envDown = L(0x3a6ab0) * 0.7f;
        p.ocean = L(0x0a2c6e); p.shallow = L(0x1a8cb4); p.land = L(0x24582a); p.land2 = L(0x8c7448); p.desert = L(0xe0b880);
        p.city = L(0xffc878); p.atmoDay = L(0x4aa0ff); p.atmoTwi = L(0xff7a40);
        p.ink = L(0xf4f7ff); p.accent = L(0x7ad8ff); p.veil = L(0x040814); p.rail = L(0x7ad8ff);
        const unsigned pc[7] = {0x3fd0ff, 0xffd23f, 0xb48cff, 0x5fe08a, 0xff5a6e, 0x4a78ff, 0xff9a3c};
        for (int i = 0; i < 7; i++) p.piece[i] = L(pc[i]);
        break;
    }
    case 1: {
        // ORBITAL DAWN: the sun rising over the limb, a red and gold line of air, the night below with its cities
        p.sunDir = normalize(vec3(-0.786f, -0.24f, -0.571f));
        p.sunC = vec3(1.f, 0.86f, 0.66f) * 2.8f; p.sunGlow = L(0xffa860); p.keyC = p.sunC;
        p.earthC = L(0x1a2846) * 0.3f; p.amb = L(0x0c0a16) * 0.7f; p.envUp = L(0x06050c); p.envDown = L(0x1a2a50) * 0.6f;
        p.ocean = L(0x061a48); p.shallow = L(0x14609a); p.land = L(0x24582a); p.land2 = L(0x8c7448); p.desert = L(0xe0b880);
        p.city = L(0xffc070); p.atmoDay = L(0x3a8aff); p.atmoTwi = L(0xff6a2a); p.cityK = 1.3f;
        p.hull = L(0xf4ece2); p.gold = L(0xf0b040); p.chase = L(0xffb060); p.lamp = L(0xffd8a0); p.window = L(0xffb870);
        p.space = L(0x03040e); p.milky = L(0x3a3a6a); p.milkyK = 0.35f; p.sunSize = 0.013f; p.bloom = 0.6f; p.ringDay = 0.38f;
        p.wrap = 0.06f; p.litDir = normalize(vec3(-0.928f, -0.174f, 0.329f)); p.airglow = L(0x40ff90) * 0.14f;
        p.ink = L(0xfff4ea); p.accent = L(0xffb870); p.veil = L(0x0a0610); p.rail = L(0xffb060);
        const unsigned pc[7] = {0x40e0d0, 0xffc84a, 0xff6ad5, 0xa8e64a, 0xff6a5a, 0x7a84ff, 0xffa040};
        for (int i = 0; i < 7; i++) p.piece[i] = L(pc[i]);
        break;
    }
    case 2: {
        // AURORA NIGHT: the night side, the cities' lights, green and violet curtains of aurora, the moon
        p.sunDir = normalize(vec3(0.25f, -0.9f, 0.35f));
        p.sunC = vec3(1.f, 0.97f, 0.92f) * 2.4f; p.sunGlow = L(0xffd0a0); p.sunVis = 0.f;
        p.keyC = vec3(0.62f, 0.72f, 1.f) * 0.95f; p.keyMoon = 1.f; p.moonSize = 0.022f;
        p.keyDir = normalize(vec3(-0.35f, 0.75f, 0.55f)); p.pole = normalize(vec3(-0.35f, 0.79f, -0.5f)); p.ringDay = 0.22f;
        p.earthC = L(0x1a2a3a) * 0.3f; p.amb = L(0x060a14) * 0.9f; p.envUp = L(0x04060c); p.envDown = L(0x101a2a) * 0.8f;
        p.ocean = L(0x061a48); p.shallow = L(0x14609a); p.land = L(0x24582a); p.land2 = L(0x8c7448); p.desert = L(0xe0b880);
        p.city = L(0xffcc80); p.atmoDay = L(0x3a6aff); p.atmoTwi = L(0xff5a30); p.cityK = 1.4f;
        p.airglow = L(0x50ff80) * 0.22f; p.auroraK = 1.3f; p.nightLift = 0.2f; p.stars = 1.f; p.milkyK = 0.5f;
        p.chase = L(0x5affd0); p.lamp = L(0xffe0b8); p.window = L(0xffc070); p.bloom = 0.65f;
        p.ink = L(0xeefcff); p.accent = L(0x5affd0); p.veil = L(0x020812); p.rail = L(0x5affd0);
        const unsigned pc[7] = {0x4af0e0, 0xf2f06a, 0xc080ff, 0x60f0a0, 0xff70a8, 0x5aa0ff, 0xffb070};
        for (int i = 0; i < 7; i++) p.piece[i] = L(pc[i]);
        break;
    }
    default: {
        // RED PLANET: a rust world, dust in the thin air (blue at its sunset), colonies' lights on its night side
        p.planetType = 1.f;
        p.litDir = normalize(vec3(0.55f, 0.7f, 0.45f));
        p.sunDir = normalize(vec3(-0.878f, -0.053f, -0.474f));
        p.keyDir = p.litDir;
        p.sunC = vec3(1.f, 0.93f, 0.85f) * 2.4f; p.sunGlow = L(0xffd0a0); p.keyC = p.sunC;
        p.earthC = L(0x8a4a28) * 0.3f; p.amb = L(0x100a0a) * 0.6f; p.envUp = L(0x06040a); p.envDown = L(0x8a4a2a) * 0.6f;
        p.land = L(0xb05430); p.land2 = L(0xcc7c48); p.ocean = L(0x4a2a24); p.desert = L(0xe2a878); p.ice = L(0xfff4ec);
        p.cloud = L(0xf0c8a0); p.clouds = 0.22f; p.atmoDay = L(0xe8a070); p.atmoTwi = L(0x4a7aff); p.city = L(0x9ae0ff);
        p.cityK = 0.8f; p.airglow = vec3(0.f); p.space = L(0x05030a); p.haze = 0.35f; p.ringDay = 0.45f;
        p.hull = L(0xf2e8de); p.gold = L(0xd88a3a); p.solar = L(0x2a1c50); p.chase = L(0xff9050); p.lamp = L(0xffe0c0);
        p.window = L(0xffd090); p.thrust = L(0xa0c0ff);
        p.landA = L(0x3a8a4a); p.landB = L(0x9ac050); p.lake = L(0x2a7ab0);
        p.ink = L(0xfff2e8); p.accent = L(0xffa060); p.veil = L(0x0c0508); p.rail = L(0xff9050);
        const unsigned pc[7] = {0x60c8ff, 0xffd880, 0xd0a0ff, 0x70e0a0, 0xff5048, 0x4868ff, 0xff8c38};
        for (int i = 0; i < 7; i++) p.piece[i] = L(pc[i]);
        break;
    }
    }
    if (p.keyMoon < 0.5f && dot(p.keyDir, p.keyDir) < 0.5f) p.keyDir = p.sunDir; // (the station lit by the sun seen, but where set)
    if (dot(p.litDir, p.litDir) < 0.5f) p.litDir = p.sunDir; // the planet lit by the sun seen (but at dawn)
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

constexpr float AZ_REF = 0.55f;     // the camera's reference heading (the variants' skies are set for it)
constexpr float CLEAR_T = 0.36f;    // a cleared row flares and shrinks into light in this long (real time)
constexpr float FLOW_T = 1.1f;      // its light flies out to the seat in this long
constexpr int MAX_MOTES = 1200;
constexpr int MAX_SPARKS = 600;

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool OrbitalUniverse::init(int width, int height) {
    progSky_ = compile(ugl::FULLSCREEN_VS, shaders::SKY_FS, "orbital sky");
    progBg_ = compile(ugl::FULLSCREEN_VS, shaders::BG_FS, "orbital bg");
    progBox_ = compilePart(shaders::BOX_VS, shaders::PART_FS, "orbital box");
    progParam_ = compilePart(shaders::PARAM_VS, shaders::PART_FS, "orbital param");
    progBoxSh_ = compilePart(shaders::BOX_VS, shaders::SHADOW_FS, "orbital box shadow");
    progParamSh_ = compilePart(shaders::PARAM_VS, shaders::SHADOW_FS, "orbital param shadow");
    progGlow_ = compile(shaders::GLOW_VS, shaders::GLOW_FS, "orbital glow");
    progBright_ = compile(ugl::FULLSCREEN_VS, shaders::BRIGHT_FS, "orbital bright");
    progBlur_ = compile(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "orbital blur");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "orbital comp");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "orbital sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "orbital veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "orbital cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "orbital shape");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "orbital post");

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
    instanced(glowVao_, glowVbo_, 3, sizeof(Glow));
    instanced(spriteVao_, spriteVbo_, 3, sizeof(SpriteInst));
    instanced(cellVao_, cellVbo_, 4, sizeof(CellInst));

    // ---- the solids: a cube, two parametric grids (fine and coarse), each with its instance buffer
    auto partInstances = [&](GLuint& inst) {
        glGenBuffers(1, &inst);
        glBindBuffer(GL_ARRAY_BUFFER, inst);
        for (int i = 0; i < 6; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Part), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    {
        std::vector<float> v;
        const int F[6][3] = {{0, 1, 2}, {0, 1, 2}, {1, 2, 0}, {1, 2, 0}, {2, 0, 1}, {2, 0, 1}};
        for (int f = 0; f < 6; f++) {
            const float sgn = (f % 2) ? -1.f : 1.f;
            const int a = F[f][0], b = F[f][1], c = F[f][2];
            const float q[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
            for (auto& e : q) {
                float pnt[3], nrm[3] = {0, 0, 0};
                pnt[a] = sgn;
                pnt[b] = e[0];
                pnt[c] = e[1] * sgn;
                nrm[a] = sgn;
                v.insert(v.end(), {pnt[0], pnt[1], pnt[2], nrm[0], nrm[1], nrm[2]});
            }
        }
        glGenVertexArrays(1, &boxVao_);
        glBindVertexArray(boxVao_);
        glGenBuffers(1, &cubeVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
        partInstances(boxInst_);
    }
    auto grid = [&](GLuint& vao, GLuint& vbo, GLuint& ibo, GLuint& inst, GLsizei& count, int NU, int NS) {
        std::vector<float> v;
        std::vector<uint32_t> idx;
        auto region = [&](int reg, int rows) {
            const uint32_t base = (uint32_t)(v.size() / 3);
            for (int r = 0; r < rows; r++)
                for (int u = 0; u <= NU; u++) v.insert(v.end(), {(float)u / NU, (float)r / (rows - 1), (float)reg});
            for (int r = 0; r + 1 < rows; r++)
                for (int u = 0; u < NU; u++) {
                    const uint32_t a = base + r * (NU + 1) + u, b = a + 1, c = a + (NU + 1), d = c + 1;
                    idx.insert(idx.end(), {a, b, d, a, d, c});
                }
        };
        region(0, 2);
        region(1, NS + 1);
        region(2, 2);
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, nullptr);
        glGenBuffers(1, &ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);
        count = (GLsizei)idx.size();
        partInstances(inst);
    };
    grid(fineVao_, fineVbo_, fineIbo_, fineInst_, fineCount_, 96, 18);
    grid(coarseVao_, coarseVbo_, coarseIbo_, coarseInst_, coarseCount_, 18, 4);

    // shapes and text (pixels)
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    // the sun's shadow map
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_N, SHADOW_N, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
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

    buildPlan();
    pal_ = basePal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void OrbitalUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    aspect_ = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    // the planet at a reduced resolution
    skyT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false, GL_NEAREST);
    resolveT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    const int qw = std::max(16, width_ / 4), qh = std::max(16, height_ / 4);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomC_.create(std::max(8, qw / 4), std::max(8, qh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomD_.create(std::max(8, qw / 4), std::max(8, qh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    if (msFbo_) {
        glDeleteFramebuffers(1, &msFbo_);
        glDeleteRenderbuffers(1, &msColor_);
        glDeleteRenderbuffers(1, &msDepth_);
    }
    GLint maxS = 4;
    glGetIntegerv(GL_MAX_SAMPLES, &maxS);
    msSamples_ = std::max(1, std::min(4, (int)maxS));
    glGenFramebuffers(1, &msFbo_);
    glGenRenderbuffers(1, &msColor_);
    glGenRenderbuffers(1, &msDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, msSamples_, GL_RGBA16F, width_, height_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, msSamples_, GL_DEPTH_COMPONENT24, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

OrbitalUniverse::~OrbitalUniverse() {
    for (ugl::Target* t : {&skyT_, &resolveT_, &bloomA_, &bloomB_, &bloomC_, &bloomD_, &sceneT_}) t->destroy();
    for (GLuint p : {progSky_, progBg_, progBox_, progParam_, progBoxSh_, progParamSh_, progGlow_, progBright_, progBlur_,
                     progComp_, progSprite_, progVeil_, progCell_, progShape_, progPost_})
        glDeleteProgram(p);
    for (GLuint v : {boxVao_, fineVao_, coarseVao_, glowVao_, spriteVao_, cellVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cubeVbo_, fineVbo_, fineIbo_, coarseVbo_, coarseIbo_, boxInst_, fineInst_, coarseInst_, glowVbo_,
                     spriteVbo_, cellVbo_, shapeVbo_})
        glDeleteBuffers(1, &b);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    if (shadowTex_) glDeleteTextures(1, &shadowTex_);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void OrbitalUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && !t.minor && t.brightness > 0.58f) v = 0;          // the day side for bright, major songs
        else if (t.hasSong && t.minor && t.brightness < 0.42f) v = 2;      // the aurora for dark ones
        else if (t.hasSong && t.bassWeight > 0.62f) v = 3;                 // the red planet for heavy ones
        else {
            static const int BY_HUE[] = {1, 0, 2, 3, 1, 0};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x0B17ull);
    if (seconds <= 0.f) palSet_ = false;
}

void OrbitalUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

void OrbitalUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

void OrbitalUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

vec2 OrbitalUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints OrbitalUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = lerp(p.space, p.ocean, 0.35f) + p.veil * 0.5f;
    h.light = false;
    h.ink = p.ink;
    h.accent = p.chase;
    h.styles = TS_INK | TS_LUMA | TS_CURTAIN;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void OrbitalUniverse::syncIds(const Game& g) {
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

vec2 OrbitalUniverse::toBoard(const vec3& w) const {
    const vec4 c = vp3_ * vec4(w - eye_, 1.f);
    if (c.w <= 1e-4f) return V(viewC_.x + viewH_.x * 0.6f, viewC_.y + viewH_.y * 0.3f);
    return V(viewC_.x + c.x / c.w * viewH_.x, viewC_.y + c.y / c.w * viewH_.y);
}

void OrbitalUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
        break;
    }
    case GameEvent::HardDrop: {
        float yLo = 99.f;
        for (auto& c : ev.cells) yLo = std::min(yLo, layout::cellPos((float)c.x, (float)c.y).y);
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > yLo + 0.01f || (int)motes_.size() >= MAX_MOTES) continue;
            Mote m;
            m.p = V(p.x + rng_.range(-0.3f, 0.3f), p.y - 0.5f);
            m.v = V(rng_.range(-0.5f, 0.5f), rng_.range(0.05f, 0.3f));
            m.life = rng_.range(0.4f, 0.7f);
            m.size = rng_.range(0.07f, 0.11f);
            m.col = lerp(pal_.piece[std::clamp(c.type, 0, 6)], vec3(1.f), 0.3f);
            m.bright = 0.7f;
            m.kind = 0;
            motes_.push_back(m);
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        const int lines = std::max(1, ev.count);
        // the light flies out to the seat of the next module
        vec2 to = V(viewC_.x + viewH_.x * 0.6f, viewC_.y + viewH_.y * 0.3f);
        const int next = std::min((int)plan_.size() - 1, built_);
        if (next >= 0 && next < (int)plan_.size()) {
            const Module& m = plan_[next];
            const Xf R = ringXf(m.ring);
            to = toBoard(R.at(m.f.at(m.pivot)));
        }
        to.x = std::clamp(to.x, -viewH_.x + 1.f, viewH_.x - 1.f);
        to.y = std::clamp(to.y, viewC_.y - viewH_.y + 1.f, viewC_.y + viewH_.y - 1.f);
        const float side = to.x >= 0.f ? 1.f : -1.f;
        for (auto& cl : ev.cells) {
            if (cl.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)cl.x, (float)cl.y);
            const int id = ids_[cl.y][cl.x];
            int msk = 0;
            if (id) {
                const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
                for (int i = 0; i < 8; i++) {
                    const int nx = cl.x + dx[i], ny = cl.y + dy[i];
                    if (nx < 0 || nx >= Game::W || ny < 0 || ny >= Game::H || !rows[ny]) continue;
                    if (ids_[ny][nx] == id) msk |= 1 << i;
                }
            }
            fading_.push_back({p.x, p.y, std::clamp(cl.type, 0, 6), msk, 0.f, seedOf(id)});
            if ((int)motes_.size() < MAX_MOTES) {
                Mote mt;
                mt.kind = 1;
                mt.from = V(p.x + rng_.range(-0.25f, 0.25f), p.y + rng_.range(-0.25f, 0.25f));
                mt.to = add(to, V(rng_.range(-0.8f, 0.8f), rng_.range(-0.6f, 0.6f)));
                mt.ctl = V(side * rng_.range(6.f, 8.f), std::max(mt.from.y, mt.to.y) + rng_.range(1.f, 3.5f));
                mt.p = mt.from;
                mt.life = FLOW_T * rng_.range(0.85f, 1.1f);
                mt.age = -rng_.range(0.f, 0.2f) - (float)(side > 0 ? Game::W - 1 - cl.x : cl.x) * 0.015f;
                mt.size = rng_.range(0.1f, 0.16f);
                mt.col = lerp(pal_.piece[std::clamp(cl.type, 0, 6)], vec3(1.f), 0.35f);
                mt.bright = 1.1f;
                motes_.push_back(mt);
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
        if (lines >= 4) quadT_ = 0.f;
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
            int msk = 0;
            const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
            for (int i = 0; i < 8; i++) {
                const int nx = c.x + dx[i], ny = c.y + dy[i];
                if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H || !full[ny][nx]) continue;
                if (id && ids_[ny][nx] == id) msk |= 1 << i;
            }
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            overCells_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), msk, (float)c.y, seedOf(id)});
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Pieces
// =====================================================================================================

void OrbitalUniverse::cell(float x, float y, float half, int mask, int type, float seed, float alpha, int mode, float erase, float glow) {
    if (alpha <= 0.003f) return;
    const vec3 tint = pal_.piece[std::clamp(type, 0, 6)];
    cells_.push_back({{x, y, half, (float)mask}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, glow, 0.f},
                      {seed, (float)std::clamp(type, 0, 6), 0.f, 0.f}});
}

void OrbitalUniverse::buildBoard(const Game& g) {
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    const float glow = 0.25f * wp_ + 0.35f * life_;
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
            cell(p.x, p.y, 0.5f, maskOf(x, y, same), c.type, seedOf(ids_[y][x]), 1.f, 0, 0.f, glow);
        }
    // game over: the stack's light drains from the top down
    if (overT_ >= 0.f)
        for (const Fading& o : overCells_) {
            const float rowFrac = (o.t - Game::HIDDEN) / (float)(Game::H - Game::HIDDEN);
            const float k = smoothstepf(rowFrac * 0.6f, rowFrac * 0.6f + 0.3f, over_);
            cell(o.x, o.y, 0.5f, o.mask, o.type, o.seed, 1.f - 0.5f * smoothstepf(0.7f, 1.f, over_), 0, 0.f, 0.f);
            cells_.back().c[3] = k;
        }
    for (const Fading& fd : fading_) cell(fd.x, fd.y, 0.5f, fd.mask, fd.type, fd.seed, 1.f, 0, saturate(fd.t / CLEAR_T), 1.f);
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
                cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), p.type, seedOf(nextId_), 0.6f, 1, 0.f, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < 0) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), p.type, seedOf(nextId_), 1.f, 0, 0.f, glow + 0.15f);
        }
    }
    auto preview = [&](int type, vec2 center, float sc, float alpha, float seed) {
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
            cell(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), type, seed, alpha, 0, 0.f, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f,
            seedOf(1000 + g.holdType()));
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f, seedOf(nextId_ + 1 + i));
    }
}

void OrbitalUniverse::sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param, float param2) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {param, param2, 0.f, 0.f}});
}

// =====================================================================================================
// The HUD
// =====================================================================================================

void OrbitalUniverse::drawHud(const std::vector<HudRect>& panels) {
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

// The lettering, by the host (text.hpp): Jura, the labels in widely spaced medium capitals, the song, the variant
// and the notices light; each line on a soft halo of space's veil, in a glow of its ink. The inks are the scene's
// (linear), toned as the post tones the scene: dimmed, their highlights rolled off, to sRGB.
void OrbitalUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* medium = text::font(FONT_MED);
    text::Font* light = text::font(FONT_LIGHT);
    const float s = height_ / 1080.f;
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount), dim = 1.f - dimT_ * 0.5f;
    auto tone = [&](const vec3& c0) {
        vec3 c = c0 * dim;
        for (float* v : {&c.x, &c.y, &c.z}) *v /= 1.f + std::max(*v - 0.8f, 0.f) * 0.8f;
        return ugl::toSrgb(c);
    };
    auto inkOf = [&](const vec3& c) { return tone(lerp(c, bridge_.ink, b)); };
    const vec3 ink = inkOf(pal_.ink), accent = inkOf(pal_.accent);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    bool full = false; // the fuller halo
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        // the name in spaced medium capitals, the variant light after it, as big
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        const float sz = base * 0.9f, tr = sz * 0.3f, gap = base * 0.9f;
        const float a = saturate(0.35f + h.alpha * 1.6f);
        put(medium, name, sz, tr, ink, a);
        if (!var.empty()) {
            // (the name's spacing stays after its last letter: the gap takes it back)
            text::Span& v = out.add(var, light, sz, accent, tr / sz, a * 0.95f);
            v.gap = (gap - tr) / sz;
        }
        break;
    }
    case HUD_SECTION: put(medium, h.text, base * 0.85f, base * 0.55f, ink, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(light, h.text, base * 1.4f, base * 0.05f, ink, h.alpha * 1.3f, -base * 0.2f); break;
    case HUD_INFO: put(medium, h.text, base * 0.68f, base * 0.25f, ink, saturate(alpha * 1.3f) * 0.85f); break;
    case HUD_LABEL: put(medium, h.text, base * 0.7f, base * 0.45f, inkOf(lerp(pal_.ink, pal_.accent, 0.5f)), saturate(alpha * 1.2f) * 0.92f); break;
    case HUD_VALUE: put(medium, h.text, base * 1.1f, base * 0.1f, ink, alpha); break;
    case HUD_NOTICE: put(light, h.text, base * 1.35f, base * 0.08f, accent, alpha); break;
    case HUD_KEY: put(medium, h.text, base * 0.78f, base * 0.12f, ink, alpha); break;
    case HUD_TOAST: put(medium, h.text, base * 1.15f, base * 0.55f, ink, alpha, -base * 0.15f); full = true; break;
    case HUD_TOAST_SUB: put(light, h.text, base * 1.2f, base * 0.1f, accent, alpha * 0.9f); full = true; break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(medium, h.text, base * 1.1f, base * 0.25f, ink, alpha, -base * 0.1f);
        else put(medium, h.text, base * 0.78f, base * 0.14f, h.accent ? accent : ink, alpha);
        full = true;
        break;
    default: put(medium, h.text, base * 0.78f, base * 0.12f, h.accent ? accent : ink, alpha); break;
    }
    // the letters as the scene drew them, on a halo of the veil, in a glow (gone with the bridge)
    text::Lettering& l = lettering_[full ? 1 : 0];
    l.glsl = shaders::LETTER_GLSL;
    l.p0 = vec4(tone(lerp(pal_.veil * 0.6f, bridge_.color, b)), full ? 0.85f : 0.6f);
    l.p1 = vec4(0.5f * (1.f - b), ugl::luma(lerp(pal_.veil, bridge_.color, b)) * dim, 0.f, 0.f);
    out.lettering = &l;
    out.look.weight = 0.2f / std::max(1.f, out.spans[0].size); // (small letters: as bold as linear blending made them)
    out.alpha = saturate(fade_);
}

// =====================================================================================================
// Camera
// =====================================================================================================

// Frames the station from its +x +z side, a little above, the planet's limb low across the screen: the station
// right of the well while it is small, the camera pulling back (a spring in log space) and the framing centering
// as it grows, looking down a little more onto the great ring.
void OrbitalUniverse::updateCamera(float dt) {
    int idx = 0;
    for (int i = 0; i < (int)plan_.size(); i++) {
        const Module& m = plan_[i];
        if (m.fading) continue;
        if (m.state == 2 || (m.state == 1 && m.t > -0.2f)) idx = i;
    }
    tanH_ = std::tan(25.f * PI / 180.f);
    // the camera's direction and the anchor's place on screen, by the framing (F: the view's half height there)
    float kMid = 0.f, kBig = 0.f, kC = 0.f, sx = 0.f, sy = 0.f, pitch = 0.f, az = 0.f, leftRoom = 0.f;
    vec3 fwd, right, upv;
    camAlt_ = std::max(2000.f, length(eye_ - EARTH_C) - R_PLANET);
    auto orient = [&](float F) {
        const float lF = std::log(std::max(F, 1.f));
        kMid = smoothstepf(std::log(35.f), std::log(130.f), lF);
        kBig = smoothstepf(std::log(600.f), std::log(10000.f), lF);
        const float kHuge = smoothstepf(std::log(1.5e5f), std::log(3.e6f), lF);
        kC = smoothstepf(std::log(250.f), std::log(5000.f), lF);
        const float kTiny = 1.f - smoothstepf(std::log(14.f), std::log(55.f), lF);
        const float dip = std::acos(R_PLANET / (R_PLANET + std::min(camAlt_, 2.e6f)));
        const float nearPitch = -dip - std::atan(-0.1f * tanH_) - kBig * 18.f * PI / 180.f;
        pitch = lerpf(nearPitch, -24.f * PI / 180.f, kHuge);
        az = 0.55f + 0.5f * kTiny + 0.04f * std::sin(time_ * 0.017f) - 0.12f * kBig + 0.12f * kHuge;
        // small, the station sits in the strip right of the previews; big, it is centered behind the well
        sx = lerpf(0.76f, 0.f, kC);
        sy = lerpf(0.12f, 0.03f, kC);
        leftRoom = lerpf(0.2f, 0.97f, kC);
        fwd = vec3(-std::sin(az) * std::cos(pitch), std::sin(pitch), -std::cos(az) * std::cos(pitch));
        right = normalize(cross(fwd, vec3(0, 1, 0)));
        upv = cross(right, fwd);
    };
    orient(camF_ > 0.f ? camF_ : 10.f);
    // what is wanted: every module in kept inside the frame around the anchor (the left side may run behind the well)
    const vec3 At = frameA_[idx];
    float need = 1.f;
    for (int i = 0; i <= idx; i++) {
        const Module& m = plan_[i];
        for (int s = 0; s < m.nsph; s++) {
            const vec3 c = vec3(m.sph[s].x, m.sph[s].y, m.sph[s].z) - At;
            const float r = m.sph[s].w, xr = dot(c, right), yu = dot(c, upv);
            need = std::max(need, (xr + r) / ((0.93f - sx) * aspect_));
            need = std::max(need, (r - xr) / (leftRoom * aspect_));
            need = std::max(need, (yu + r) / (0.86f - sy));
            need = std::max(need, (r - yu) / (0.92f + sy));
        }
    }
    const float Ft = need * 1.06f;
    if (camF_ < 0.f) {
        camF_ = Ft;
        camFv_ = 0.f;
        camA_ = At;
    }
    {
        const float w = 1.5f;
        float lf = std::log(camF_);
        camFv_ += (w * w * (std::log(Ft) - lf) - 2.f * w * camFv_) * dt;
        lf += camFv_ * dt;
        camF_ = std::exp(lf);
        const float k = 1.f - std::exp(-dt * 1.4f);
        // the anchor never runs ahead of the framing (it moves at most a few frames' widths a second)
        // (its speed eased, so it never starts or stops with a jolt)
        vec3 want = (At - camA_) * 1.4f;
        const float wl = length(want), maxV = camF_ * 1.6f;
        if (wl > maxV && wl > 0.f) want = want * (maxV / wl);
        camAv_ = camAv_ + (want - camAv_) * (1.f - std::exp(-dt * 2.5f));
        camA_ = camA_ + camAv_ * dt;
        (void)k;
    }
    const float F = camF_;
    orient(F);
    camAz_ = az;
    const vec3 dirA = normalize(fwd + right * (sx * tanH_ * aspect_) + upv * (sy * tanH_));
    const float D = F / tanH_ * (1.f + 0.7f * away_ * away_);
    eye_ = camA_ - dirA * D;
    {
        // never inside the planet's air
        const vec3 r = eye_ - EARTH_C;
        const float d = length(r), minD = R_PLANET + 1.5e5f;
        if (d < minD) eye_ = EARTH_C + r * (minD / d);
    }
    look_ = fwd;
    camR_ = right;
    camU_ = upv;
    const float zn = std::max(0.05f, D * 0.03f);
    const float zf = std::max({D * 30.f + F * 8.f, length(eye_ - EARTH_C) + R_PLANET * 1.5f, length(eye_ - MOON_C) + R_MOON * 2.f});
    const mat4 view3 = mat4::lookAt(vec3(0.f), fwd, vec3(0, 1, 0));
    vp3_ = mat4::perspective(2.f * std::atan(tanH_), aspect_, zn, zf) * view3;
    minPx_ = 2.f * tanH_ / (float)height_;
}

// =====================================================================================================
// Frame
// =====================================================================================================

void OrbitalUniverse::uploadParts() {
    glBindBuffer(GL_ARRAY_BUFFER, boxInst_);
    glBufferData(GL_ARRAY_BUFFER, boxes_.size() * sizeof(Part), boxes_.empty() ? nullptr : boxes_.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, fineInst_);
    glBufferData(GL_ARRAY_BUFFER, fine_.size() * sizeof(Part), fine_.empty() ? nullptr : fine_.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, coarseInst_);
    glBufferData(GL_ARRAY_BUFFER, coarse_.size() * sizeof(Part), coarse_.empty() ? nullptr : coarse_.data(), GL_STREAM_DRAW);
}

void OrbitalUniverse::drawParts(GLuint progBox, GLuint progParam, bool shadow) {
    (void)shadow;
    if (!boxes_.empty()) {
        glUseProgram(progBox);
        glBindVertexArray(boxVao_);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 36, (GLsizei)boxes_.size());
    }
    if (!fine_.empty()) {
        glUseProgram(progParam);
        glBindVertexArray(fineVao_);
        glDrawElementsInstanced(GL_TRIANGLES, fineCount_, GL_UNSIGNED_INT, nullptr, (GLsizei)fine_.size());
    }
    if (!coarse_.empty()) {
        glUseProgram(progParam);
        glBindVertexArray(coarseVao_);
        glDrawElementsInstanced(GL_TRIANGLES, coarseCount_, GL_UNSIGNED_INT, nullptr, (GLsizei)coarse_.size());
    }
}

void OrbitalUniverse::render(const Frame& f, GLuint fbo) {
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
    wm_ = smoothstepf(0.2f, 1.f, lv_);
    wp_ = smoothstepf(1.1f, 2.f, lv_);
    // a real drop, not a chorus (both rise into a peak): from the HUD's section, else from the music's pace
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = dropMode_ && overT_ < 0.f && (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f);
    if (isDrop && !wasDrop_) dropSeq_ = 0.f;
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.2f : 0.35f, dt);
    life_ = smoothstepf(0.f, 1.f, dropAmt_);
    dropSeq_ = isDrop ? std::min(1.3f, dropSeq_ + dt * 0.5f) : approach(dropSeq_, 0.f, 0.25f, dt);
    halo_ = approach(halo_, isDrop ? 1.f : 0.f, isDrop ? 0.5f : 0.3f, dt);
    groupOn_[0] = 1.f;
    groupOn_[1] = approach(groupOn_[1], lv_ > 0.6f ? 1.f : 0.f, 1.2f, dt);
    groupOn_[2] = approach(groupOn_[2], lv_ > 1.5f ? 1.f : 0.f, 1.2f, dt);
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    quadT_ += dt;
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    kickGlow_ = std::max(approach(kickGlow_, 0.f, 4.f, dt), kick * wp_);
    if (music.hasSong) beat_ = music.beat;
    else if (!f.paused) beat_ += dt * 2.0;
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
        // the sky turns with the camera's heading: each variant's sun (moon, pole) is set for the reference heading
        // (AZ_REF), so the composition holds as the camera swings round the growing station (the sun just over the
        // limb left of the well at dawn); the solar wings, the shadows and the planet's light all follow
        const float a0 = AZ_REF, a1 = camAz_;
        const vec3 r0(std::cos(a0), 0.f, -std::sin(a0)), f0(-std::sin(a0), 0.f, -std::cos(a0));
        const vec3 r1(std::cos(a1), 0.f, -std::sin(a1)), f1(-std::sin(a1), 0.f, -std::cos(a1));
        auto turn = [&](vec3& v) { v = r1 * dot(v, r0) + f1 * dot(v, f0) + vec3(0.f, v.y, 0.f); };
        turn(pal_.sunDir);
        turn(pal_.litDir);
        turn(pal_.keyDir);
        turn(pal_.moonDir);
        turn(pal_.pole);
    }
    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);

    // ---- The game's lines -> the modules (a new game takes them back to the capsule).
    {
        static const int testLines = std::getenv("REFALL_ORBITAL_LINES") ? std::atoi(std::getenv("REFALL_ORBITAL_LINES")) : 0;
        // (REFALL_GROW_RATE=R adds R more lines a second: a time-lapse)
        static const double growRate = std::getenv("REFALL_GROW_RATE") ? std::atof(std::getenv("REFALL_GROW_RATE")) : 0.0;
        // (on the 50-line scale: the song's pace, paceLines; a break: the station as it was)
        const float lines = f.bare ? lines_ : paceLines(game, (float)(testLines + (int)(growRate * time_)));
        const int want = std::clamp(wantOf(lines), 1, (int)plan_.size());
        if (linesShown_ < 0) {
            for (int i = 0; i < (int)plan_.size(); i++) {
                plan_[i].state = i < want ? 2 : 0;
                plan_[i].present = i < want ? 1.f : 0.f;
                plan_[i].t = 0.f;
                plan_[i].fading = false;
            }
            for (int r = 1; r < RINGS; r++)
                if (rings_[r].first >= 0) rings_[r].w = want > rings_[r].last ? rings_[r].baseW : 0.f;
            built_ = want;
        } else {
            // a new game: everything beyond what is wanted shrinks away, the outermost first
            int maxIdx = 0;
            for (int i = 0; i < (int)plan_.size(); i++)
                if (plan_[i].state != 0) maxIdx = i;
            for (int i = want; i <= maxIdx; i++)
                if (plan_[i].state != 0 && !plan_[i].fading) {
                    plan_[i].fading = true;
                    plan_[i].fadeT = -std::min(1.6f, (maxIdx - i) * 0.04f);
                }
            // new lines: the next modules fly in, one after the other (a module still shrinking away waits), a big
            // batch (a quad late on: some fifty) closer together so that it is all on its way within ~9 s
            int n = 0;
            for (int i = 1; i < want; i++) n += plan_[i].state == 0 ? 1 : 0;
            const float gap = std::min(0.45f, 9.f / (float)std::max(1, n));
            int k = 0;
            for (int i = 1; i < want; i++)
                if (plan_[i].state == 0) startFlight(i, k++ * gap);
            built_ = want;
        }
        lines_ = lines;
        linesShown_ = lines;
    }
    // flights, latches, fades
    bool armBusy = false;
    for (const Module& m : plan_)
        if (m.state == 1 && m.armed) armBusy = true;
    for (int i = 0; i < (int)plan_.size(); i++) {
        Module& m = plan_[i];
        if (m.fading && m.state != 0) {
            m.fadeT += wdt;
            m.present = 1.f - smoothstepf(0.f, 0.9f, m.fadeT);
            if (m.fadeT > 0.95f) { m.state = 0; m.present = 0.f; m.fading = false; m.armed = false; }
            continue;
        }
        if (m.state == 1) {
            m.t += wdt;
            if (!m.armed && !armBusy && m.armOk && m.t >= 0.f && m.t < flightA(m) - 1.9f && plan_[2].state == 2) {
                m.armed = true;
                armBusy = true;
            }
            if (m.t >= flightA(m) + flightB(m)) {
                m.state = 2;
                m.latch = 1.f;
                m.armed = false;
                // a few sparks at the seat
                const Xf mf = moduleXf(m, nullptr, nullptr, nullptr);
                const vec3 seat = mf.at(m.pivot) - ringXf(m.ring).dir(m.approach) * (m.extent * 0.45f);
                for (int s = 0; s < 18 && (int)sparks_.size() < MAX_SPARKS; s++) {
                    Spark sp;
                    sp.p = seat;
                    const vec3 dir = normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f)));
                    sp.v = dir * (m.extent * rng_.range(0.2f, 0.7f));
                    sp.age = 0.f;
                    sp.life = rng_.range(0.4f, 0.9f);
                    sp.size = m.extent * rng_.range(0.012f, 0.025f);
                    sp.col = rng_.chance(0.5f) ? vec3(1.f, 0.85f, 0.6f) : vec3(0.7f, 0.85f, 1.f);
                    sparks_.push_back(sp);
                }
            }
        }
        m.latch = approach(m.latch, 0.f, 1.4f, wdt);
    }
    // the rings turn once complete: slowly, faster at a peak, spun up on the drop (some against the others)
    for (int r = 1; r < RINGS; r++) {
        Ring& RG = rings_[r];
        if (RG.first < 0) continue;
        bool all = true;
        for (int i = RG.first; i <= RG.last && all; i++) all = plan_[i].state == 2 && !plan_[i].fading;
        const float target = all ? RG.baseW + RG.peakW * (r == 1 ? 0.6f * wm_ + wp_ : wp_) + RG.dropW * life_ : 0.f;
        RG.w = approach(RG.w, target, r == 1 ? 0.45f : 0.35f, dt);
        RG.spin = std::fmod(RG.spin + RG.w * wdt, TAU);
    }
    wheelSpin_ = rings_[1].spin;
    greatSpin_ = rings_[2].spin;
    planetSpin_ += wdt * 0.0012f;
    cloudSpin_ += wdt * 0.0016f;

    // the scene's setup and teardown: the station powered up from the core outward as the camera eases in, then
    // powered down from its rim inward as it draws back
    powerUp_ = smoothstepf(0.1f, 0.75f, f.setup);
    powerDown_ = smoothstepf(0.05f, 0.8f, f.teardown);
    away_ = std::max(1.f - smoothstepf(0.f, 0.8f, f.setup), smoothstepf(0.1f, 1.f, f.teardown));
    updateCamera(dt);

    // ---- Sparks: welding on the beat grid at a peak (every two beats), the kicks adding a few.
    {
        const int half = (int)std::floor(beat_ / 2.0);
        const bool beatHit = half != lastHalf_;
        lastHalf_ = half;
        if (beatHit && wp_ > 0.3f && overT_ < 0.f && !f.paused) {
            int docked = 0;
            for (const Module& m : plan_) docked += m.state == 2 ? 1 : 0;
            if (docked > 1) {
                const int pick = rng_.irange(0, docked - 1);
                int k = 0;
                for (const Module& m : plan_) {
                    if (m.state != 2) continue;
                    if (k++ != pick) continue;
                    const Xf mf = moduleXf(m, nullptr, nullptr, nullptr);
                    const vec3 at = mf.at(m.pivot) + normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(-0.2f, 1.f), rng_.range(-1.f, 1.f))) * (m.extent * 0.4f);
                    const int n = 8 + (int)(10.f * kick);
                    for (int s = 0; s < n && (int)sparks_.size() < MAX_SPARKS; s++) {
                        Spark sp;
                        sp.p = at;
                        sp.v = normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f))) * (m.extent * rng_.range(0.1f, 0.35f));
                        sp.age = 0.f;
                        sp.life = rng_.range(0.3f, 0.7f);
                        sp.size = std::min(m.extent, camF_ * 0.2f) * rng_.range(0.01f, 0.02f);
                        sp.col = rng_.chance(0.6f) ? vec3(1.f, 0.9f, 0.7f) : vec3(0.7f, 0.85f, 1.f);
                        sparks_.push_back(sp);
                    }
                    break;
                }
            }
        }
        sparkAcc_ += wdt * (2.5f + 5.f * wm_ + 8.f * wp_) * smoothstepf(30.f, 200.f, camF_);
        while (sparkAcc_ >= 1.f) {
            sparkAcc_ -= 1.f;
            int last[4] = {-1, -1, -1, -1}, nl = 0;
            for (int i = (int)plan_.size() - 1; i >= 0 && nl < 4; i--)
                if (plan_[i].state == 2 && !plan_[i].fading && plan_[i].extent > camF_ * 0.03f) last[nl++] = i;
            if (nl == 0) break;
            const Module& m = plan_[last[rng_.irange(0, nl - 1)]];
            const Xf mf = moduleXf(m, nullptr, nullptr, nullptr);
            const vec3 at = mf.at(m.pivot) + normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f))) * (m.extent * rng_.range(0.2f, 0.6f));
            for (int s = 0; s < 7 && (int)sparks_.size() < MAX_SPARKS; s++) {
                Spark sp;
                sp.p = at;
                sp.v = normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f))) * (camF_ * rng_.range(0.01f, 0.04f));
                sp.age = 0.f;
                sp.life = rng_.range(0.3f, 0.7f);
                sp.size = camF_ * rng_.range(0.002f, 0.004f);
                sp.col = rng_.chance(0.6f) ? vec3(1.f, 0.9f, 0.7f) : vec3(0.7f, 0.85f, 1.f);
                sparks_.push_back(sp);
            }
        }
        for (Spark& s : sparks_) {
            s.age += wdt;
            s.p += s.v * wdt;
        }
        sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Spark& s) { return s.age >= s.life; }), sparks_.end());
    }

    // ---- 2D motes over the board, the clearing cells.
    for (Mote& m : motes_) {
        m.age += dt;
        if (m.kind == 1) {
            const float t = smoothstepf(0.f, 1.f, saturate(m.age / m.life));
            const vec2 a = lerp2(m.from, m.ctl, t), b = lerp2(m.ctl, m.to, t);
            m.p = lerp2(a, b, t);
        } else {
            m.p = add(m.p, mul(m.v, dt));
            m.v = mul(m.v, std::exp(-2.5f * dt));
        }
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.age >= m.life; }), motes_.end());
    for (auto& fd : fading_) fd.t += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > CLEAR_T; }), fading_.end());
    sprites_.clear();
    for (const Mote& m : motes_) {
        if (m.age < 0.f || f.bare) continue; // (the motes over the board: none in a break)
        const float t = m.age / m.life;
        float a = smoothstepf(0.f, 0.12f, t) * (1.f - smoothstepf(0.6f, 1.f, t)) * m.bright;
        if (m.kind == 1) a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.85f, 1.f, t)) * m.bright;
        sprite(m.p, m.size * (m.kind == 1 ? 2.f : 1.6f), m.col, a * world, 0);
    }
    cells_.clear();
    if (!f.bare) buildBoard(game); // (a break: the world alone)

    // ---- The station's solids and lights.
    boxes_.clear();
    fine_.clear();
    coarse_.clear();
    glows_.clear();
    const float emitK = (0.55f + 0.3f * groupOn_[1] + 0.25f * groupOn_[2] + 0.3f * slow_ + 0.35f * std::max(0.f, 1.f - quadT_ / 2.5f)) *
                        (1.f - 0.85f * std::max(over_, std::max(powerDown_, 1.f - powerUp_)));
    {
        Builder b(boxes_, fine_, coarse_, glows_);
        b.minR = camF_ * 0.012f;
        for (int i = 0; i < (int)plan_.size(); i++) {
            Module& m = plan_[i];
            if (m.state == 0 || m.present <= 0.002f) continue;
            if (m.state == 1 && m.t < 0.f) continue;
            float acc = 0.f;
            vec3 tugP(0.f);
            bool tugOn = false;
            b.xf = moduleXf(m, &acc, &tugP, &tugOn);
            b.pivot = m.pivot;
            b.s = m.present;
            b.emitK = emitK;
            b.glowK = 1.f;
            b.towns = 0.f;
            b.chaseOff = m.ring > 0 ? -rings_[m.ring].spin : 0.f;
            const vec3 cw = b.xf.at(m.pivot);
            m.order = saturate(length(cw - frameA_[0]) / std::max(camF_ * 1.1f, 1.f));
            emitModule(b, m, i);
            const vec3 dir = ringXf(m.ring).dir(m.approach);
            // its tugs: pushing it in, then backing off and leaving
            if (m.state == 1 && m.tugs > 0) {
                const float TA = flightA(m);
                const float size = std::max(2.4f, m.extent * (m.kind >= K_NRING ? 0.045f : 0.16f));
                vec3 u, v;
                basisOf(dir, u, v);
                for (int k = 0; k < m.tugs; k++) {
                    const float ang = m.tugs == 1 ? 0.f : TAU * k / m.tugs + m.seed * 3.f;
                    const vec3 side = m.tugs == 1 ? vec3(0.f) : (u * std::cos(ang) + v * std::sin(ang)) * (m.extent * 0.55f);
                    const vec3 at0 = tugP + dir * (m.extent * (m.tugs == 1 ? 0.95f : 0.4f) + size * 0.7f) + side;
                    if (m.t < TA) {
                        const float thrust = std::fabs(acc);
                        emitTug(b, at0, dir * -1.f, size, acc >= 0.f ? dir : dir * -1.f, 0.25f + thrust, 1.f, m.seed + k);
                    } else {
                        const float tt = m.t - TA;
                        if (tt < 5.f) {
                            const vec3 away = normalize(dir * 0.8f + normalize(side + u * 0.01f) * 0.6f + camU_ * 0.2f);
                            const float d = 0.5f * tt * tt * (size * 3.f + m.extent * 0.6f);
                            emitTug(b, at0 + away * d, dir * -1.f, size, away * -1.f, 0.9f, 1.f - smoothstepf(3.5f, 5.f, tt), m.seed + k);
                        }
                    }
                }
            }
            // a self-propelled ship: its own plumes as it comes
            if (m.state == 1 && m.tugs == 0 && m.t < flightA(m)) {
                b.xf = Xf();
                b.pivot = vec3(0.f);
                b.s = 1.f;
                const float th = std::fabs(acc);
                const vec3 back = cw + dir * (m.extent * 0.85f);
                const vec3 ex = acc >= 0.f ? dir : dir * -1.f;
                b.light(back, m.extent * 0.25f, pal_.thrust, 0.4f + 1.4f * th, 2, ex);
            }
            // the docking's light at the seat
            if (m.latch > 0.01f) {
                b.xf = Xf();
                b.pivot = vec3(0.f);
                b.s = 1.f;
                const vec3 seat = cw - dir * (m.extent * 0.45f);
                b.light(seat, std::max(0.6f, m.extent * 0.22f), lerp(pal_.lamp, vec3(1.f), 0.5f), m.latch * 1.6f);
            }
        }
        b.chaseOff = 0.f;
        {
            int works = 0;
            for (int i = stageFirst_[6]; i < stageFirst_[7] && i < (int)plan_.size(); i++) works += plan_[i].state == 2 && !plan_[i].fading ? 1 : 0;
            moonWork_ = approach(moonWork_, works / 6.f, 0.5f, dt);
        }
        emitArm(b, dt);
        emitDrones(b);
        emitMoon(b);
        emitTraffic(b);
        // sparks
        b.xf = Xf();
        b.pivot = vec3(0.f);
        b.s = 1.f;
        for (const Spark& s : sparks_) {
            const float t = s.age / s.life;
            b.light(s.p, s.size, s.col, (1.f - t) * 2.f, 0);
        }
    }
    uploadParts();

    // =========================================================== the planet and space (reduced resolution)
    const float sunVis = pal_.sunVis * (1.f - 0.85f * slow_);
    const vec3 keyDir = normalize(pal_.keyDir);
    const vec3 keyC = pal_.keyC * pal_.keyK * (1.f - 0.85f * slow_) * (1.f + 0.1f * wp_ + 0.12f * life_);
    {
        skyT_.bind();
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(progSky_);
        set3f(progSky_, "uLook", look_);
        set3f(progSky_, "uCamR", camR_);
        set3f(progSky_, "uCamU", camU_);
        set2f(progSky_, "uTan", tanH_ * aspect_, tanH_);
        set1f(progSky_, "uPix", 2.f * tanH_ / skyT_.h);
        const vec3 eyeP = eye_ - EARTH_C;
        const float alt = std::max(10.f, length(eyeP) - R_PLANET);
        set3f(progSky_, "uEyeP", eyeP);
        set1f(progSky_, "uAlt", alt);
        set1f(progSky_, "uDyson", saturate((float)(built_ - stageFirst_[5]) / 3.f) * (stageFirst_[5] > 0 ? 1.f : 0.f));
        set1f(progSky_, "uR", R_PLANET);
        set3f(progSky_, "uSunDir", pal_.sunDir);
        set3f(progSky_, "uLitDir", normalize(pal_.litDir));
        set3f(progSky_, "uSunC", pal_.sunC * (1.f / 2.6f) * (1.f - 0.5f * slow_));
        set3f(progSky_, "uSunGlow", pal_.sunGlow);
        set1f(progSky_, "uSunSize", pal_.sunSize);
        set1f(progSky_, "uSunVis", sunVis);
        set3f(progSky_, "uSpaceC", pal_.space);
        set3f(progSky_, "uMilkyC", pal_.milky);
        set1f(progSky_, "uMilky", pal_.milkyK);
        {
            // the planet turning under the station about a tilted axis (its pole beyond the far horizon)
            const vec3 A = normalize(pal_.pole);
            vec3 e1, e2;
            basisOf(A, e1, e2);
            auto rotM = [&](float ang, float* m9) {
                const vec3 ex = e1 * std::cos(ang) + e2 * std::sin(ang);
                const vec3 ez = e2 * std::cos(ang) - e1 * std::sin(ang);
                const float v[9] = {ex.x, A.x, ez.x, ex.y, A.y, ez.y, ex.z, A.z, ez.z};
                std::memcpy(m9, v, sizeof(v));
            };
            float m9[9];
            rotM(planetSpin_ + (float)(seed_ % 1000) * 0.0063f, m9);
            glUniformMatrix3fv(ugl::U(progSky_, "uPlanetRot"), 1, GL_FALSE, m9);
            rotM(cloudSpin_ + (float)(seed_ % 1000) * 0.0063f + 1.3f, m9);
            glUniformMatrix3fv(ugl::U(progSky_, "uCloudRot"), 1, GL_FALSE, m9);
        }
        set1f(progSky_, "uType", pal_.planetType);
        set3f(progSky_, "uOcean", pal_.ocean);
        set3f(progSky_, "uShallow", pal_.shallow);
        set3f(progSky_, "uLand", pal_.land);
        set3f(progSky_, "uLand2", pal_.land2);
        set3f(progSky_, "uDesert", pal_.desert);
        set3f(progSky_, "uIce", pal_.ice);
        set3f(progSky_, "uCloudC", pal_.cloud);
        set3f(progSky_, "uCityC", pal_.city);
        set3f(progSky_, "uAtmoDay", pal_.atmoDay);
        set3f(progSky_, "uAtmoTwi", pal_.atmoTwi);
        set3f(progSky_, "uAirglow", pal_.airglow);
        set1f(progSky_, "uClouds", pal_.clouds);
        set1f(progSky_, "uCity", pal_.cityK * (1.f + 0.6f * slow_));
        set1f(progSky_, "uAurora", pal_.auroraK * (1.f + 0.3f * wp_ + 0.3f * life_));
        set3f(progSky_, "uAurora1", pal_.aurora1);
        set3f(progSky_, "uAurora2", pal_.aurora2);
        set1f(progSky_, "uGlow", 0.3f * wp_ + 0.5f * life_ + 0.3f * swell_);
        set1f(progSky_, "uNightLift", pal_.nightLift);
        set1f(progSky_, "uHaze", pal_.haze);
        set1f(progSky_, "uWrap", pal_.wrap);
        set3f(progSky_, "uMoonDir", pal_.moonDir);
        set1f(progSky_, "uMoonSize", pal_.moonSize);
        set1f(progSky_, "uTime", time_);
        set1f(progSky_, "uBridge", bridge);
        set3f(progSky_, "uBridgeCol", bridge_.color);
        ugl::drawFullscreen();
    }

    // ---- The sun's place on screen for the lens flare: in front of the camera, not behind the planet (eased out as
    // it sinks under the limb's air), faded at the frame's edges; what the station hides of it COMP_FS measures.
    {
        const vec3 sd = normalize(pal_.sunDir), E = eye_ - EARTH_C;
        const float z = dot(sd, look_);
        float k = 0.f;
        if (z > 0.05f) {
            const vec2 ndc{dot(sd, camR_) / z / (tanH_ * aspect_), dot(sd, camU_) / z / tanH_};
            sunUv_ = vec2{ndc.x * 0.5f + 0.5f, ndc.y * 0.5f + 0.5f};
            k = (1.f - smoothstepf(0.95f, 1.3f, std::fabs(ndc.x))) * (1.f - smoothstepf(0.95f, 1.3f, std::fabs(ndc.y)));
            const float b = dot(E, sd);
            if (b < 0.f) {
                // the sun's height over the limb (its ray's closest pass over the ground, less the bright air) as an angle
                const float hm = std::sqrt(std::max(dot(E, E) - b * b, 0.f)) - R_PLANET - 15000.f;
                k *= smoothstepf(-pal_.sunSize, pal_.sunSize * 1.5f, hm / -b);
            }
        }
        flare_ = k * sunVis * world;
    }

    // =========================================================== the sun's shadow map
    vec3 shC = camA_;
    float shR = camF_ * 1.6f;
    mat4 shVP;
    {
        const vec3 up = std::fabs(keyDir.y) < 0.95f ? vec3(0, 1, 0) : vec3(1, 0, 0);
        const mat4 view = mat4::lookAt(keyDir * (shR * 2.f), vec3(0.f), up);
        shVP = mat4::ortho(-shR, shR, -shR, shR, 0.01f * shR, shR * 4.f) * view;
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glViewport(0, 0, SHADOW_N, SHADOW_N);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        for (GLuint p : {progBoxSh_, progParamSh_}) {
            glUseProgram(p);
            setMat(p, "uVP", shVP);
            set3f(p, "uEye", shC);
        }
        drawParts(progBoxSh_, progParamSh_, true);
    }

    // =========================================================== the station (full resolution, multisampled)
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUseProgram(progBg_);
    {
        const vec3 eyeP = eye_ - EARTH_C;
        set3f(progBg_, "uEyeP", eyeP);
        set1f(progBg_, "uAlt", std::max(10.f, length(eyeP) - R_PLANET));
        set1f(progBg_, "uR", R_PLANET);
        setMat(progBg_, "uVP", vp3_);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skyT_.tex);
    set1i(progBg_, "uSky", 0);
    set3f(progBg_, "uLook", look_);
    set3f(progBg_, "uCamR", camR_);
    set3f(progBg_, "uCamU", camU_);
    set2f(progBg_, "uTan", tanH_ * aspect_, tanH_);
    set2f(progBg_, "uRes", (float)width_, (float)height_);
    set1f(progBg_, "uTwinkle", 0.2f + 0.15f * wm_ + 0.2f * wp_);
    set1f(progBg_, "uTime", time_);
    set1f(progBg_, "uStars", pal_.stars * (1.f + 0.6f * slow_));
    ugl::drawFullscreen();
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    if (msSamples_ > 1) glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    {
        const float dip = std::acos(R_PLANET / (R_PLANET + camAlt_));
        const vec3 envDown = pal_.envDown * (1.f - 0.4f * slow_);
        for (GLuint p : {progBox_, progParam_}) {
            glUseProgram(p);
            setMat(p, "uVP", vp3_);
            set3f(p, "uEye", eye_);
            set3f(p, "uKeyDir", keyDir);
            set3f(p, "uKeyC", keyC);
            set3f(p, "uEarthC", pal_.earthC * (1.f - 0.4f * slow_));
            set3f(p, "uAmbC", pal_.amb);
            set3f(p, "uEnvUp", pal_.envUp);
            set3f(p, "uEnvDown", envDown);
            set1f(p, "uDipS", std::sin(dip));
            set3f(p, "uPlanetC", EARTH_C);
            set1f(p, "uPlanetR", R_PLANET);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, shadowTex_);
            set1i(p, "uShadow", 1);
            // the shadow's matrix maps world points relative to its own origin
            setMat(p, "uShadowVP", shVP);
            set1f(p, "uShadowTexel", 1.f / SHADOW_N);
            set1f(p, "uShadowBias", 2.f * shR / SHADOW_N * 1.5f);
            set1f(p, "uTime", time_);
            set1f(p, "uLights", 0.4f + 0.6f * groupOn_[1] + 0.4f * slow_);
            set1f(p, "uBridge", bridge);
            set3f(p, "uBridgeCol", bridge_.color);
            set3f(p, "uWindowC", pal_.window * 1.6f);
            set3f(p, "uTownC", pal_.town);
            set3f(p, "uLake", pal_.lake);
            set3f(p, "uRock", pal_.rock);
            set3f(p, "uCloudC", pal_.ringCloud);
            set3f(p, "uLandA", pal_.landA);
            set3f(p, "uLandB", pal_.landB);
            set3f(p, "uShadowO", shC);
            set1f(p, "uRingDay", pal_.ringDay);
            {
                const float h1 = (float)std::fmod(beat_ / 8.0, 1.0) * TAU, h2 = (float)std::fmod(beat_ / 16.0, 1.0) * TAU;
                set4f(p, "uChase", h1, h2, life_ * (1.f - over_), (0.12f + 0.35f * groupOn_[1] + 0.25f * kickGlow_) * (1.f - over_));
                set3f(p, "uChaseC", pal_.chase * 1.4f);
            }
        }
        glActiveTexture(GL_TEXTURE0);
        drawParts(progBox_, progParam_, false);
    }
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    // the lights: additive, depth tested
    if (!glows_.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthMask(GL_FALSE);
        glUseProgram(progGlow_);
        setMat(progGlow_, "uVP", vp3_);
        set3f(progGlow_, "uEye", eye_);
        set3f(progGlow_, "uCamR", camR_);
        set3f(progGlow_, "uCamU", camU_);
        set1f(progGlow_, "uMinPx", minPx_);
        // fade with the bridge
        for (Glow& g : glows_) g.c[3] *= world;
        glBindVertexArray(glowVao_);
        glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
        glBufferData(GL_ARRAY_BUFFER, glows_.size() * sizeof(Glow), glows_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)glows_.size());
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // ---- The bloom (a quarter and a sixteenth of the resolution).
    bloomA_.bind();
    glUseProgram(progBright_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resolveT_.tex);
    set1i(progBright_, "uSrc", 0);
    set2f(progBright_, "uTexel", 1.f / resolveT_.w * 1.5f, 1.f / resolveT_.h * 1.5f);
    set1f(progBright_, "uThreshold", 0.9f);
    ugl::drawFullscreen();
    auto blur = [&](ugl::Target& a, ugl::Target& b2, int passes) {
        for (int pass = 0; pass < passes; pass++) {
            b2.bind();
            glUseProgram(progBlur_);
            glBindTexture(GL_TEXTURE_2D, a.tex);
            set1i(progBlur_, "uSrc", 0);
            set2f(progBlur_, "uDir", 1.f / a.w, 0.f);
            ugl::drawFullscreen();
            a.bind();
            glBindTexture(GL_TEXTURE_2D, b2.tex);
            set2f(progBlur_, "uDir", 0.f, 1.f / a.h);
            ugl::drawFullscreen();
        }
    };
    blur(bloomA_, bloomB_, 1);
    bloomC_.bind();
    glUseProgram(progBright_);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    set2f(progBright_, "uTexel", 1.f / bloomA_.w, 1.f / bloomA_.h);
    set1f(progBright_, "uThreshold", 0.f);
    ugl::drawFullscreen();
    blur(bloomC_, bloomD_, 2);

    // =========================================================== the scene (full resolution)
    sceneT_.bind();
    glUseProgram(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resolveT_.tex);
    set1i(progComp_, "uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    set1i(progComp_, "uBloomT", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, bloomC_.tex);
    set1i(progComp_, "uBloom2", 2);
    glActiveTexture(GL_TEXTURE0);
    set1f(progComp_, "uBloomK", pal_.bloom * (1.f + 0.25f * wp_ + 0.3f * life_ + 0.2f * swell_) * world);
    set1f(progComp_, "uSat", 1.08f);
    set2f(progComp_, "uSunUv", sunUv_.x, sunUv_.y);
    set1f(progComp_, "uFlare", flare_ * (1.f + 0.15f * wp_ + 0.25f * life_));
    set3f(progComp_, "uFlareC", lerp(pal_.sunGlow, vec3(1.f), 0.35f));
    set1f(progComp_, "uAspect", aspect_);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    // ---- The veil behind the well and the HUD's columns (none in a break).
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", lerp(pal_.veil, bridge_.color, bridge));
        set3f(progVeil_, "uRail", lerp(pal_.rail * 0.45f, bridge_.ink * 0.4f, bridge));
        set1f(progVeil_, "uWellA", 0.84f);
        set1f(progVeil_, "uColA", 0.66f * world);
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
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_orbital() {
    static const UniverseEntry e{
        "ORBITAL STATION", "a space station building itself in orbit, modules docked with every cleared line, from a lone capsule to a great forest ring",
        [] { return std::unique_ptr<Universe>(new OrbitalUniverse()); },
        // Fits most songs; the spacious, steady ones a little better.
        [](const SongLook& l) { return 0.85f + 0.15f * (1.f - l.density) + 0.1f * l.dynamics; },
        [] {
            std::vector<std::string> v;
            for (const char* n : VARIANT_NAMES) v.push_back(n);
            return v;
        }()};
    return e;
}
