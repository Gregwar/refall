// LOST PANDA: a journey home told in pictures behind the well, one continuous shot. A little kawaii panda cub is lost
// in the city at night, alone under the rain. Every cleared line is a few steps further on its way home, through one
// world the camera follows it through, never cutting: the rainy street becomes the shopping street, a cartoon night
// city in layers of parallax (a moon, misty skyscrapers, towers, an elevated line, brick buildings with warm windows
// and neon signs, black silhouettes at the front); the cub goes down the subway's
// stairs (the world cut away like a cross-section, the street still above, pipes and cables in the earth between);
// waits on the platform (tiles, graffiti, vending machines, commuters) while a silver train glides in, steps in;
// the train pulls out (the camera holds back a moment, then catches up), gathers speed hard and runs flat out under
// the city through a tunnel over a thousand units long (the street whipping past above, the tunnel's lamps streaking
// past its windows), climbs a cutting into the dawn and brakes into a terminus in the industrial district (chimneys
// banded red and white trailing smoke, gas holders, cranes, works and warehouses, stacked containers, a chain-link
// fence); the cub steps out, the train leaves; it walks on along the canal (cobbles, coping stones, the sodium lamps
// wavering in the water), past the last fences into the fields (misty mountains, rolling hills in a patchwork of
// fields, farms, pylons, flowers, a butterfly), the first bamboo, the bamboo forest (after a reference: far culms
// dissolving in a turquoise mist, sunlit leaf sprays, stone lanterns grown with moss, ferns, mossy rocks, big dark
// culms framing the view), over a little bridge across a stream and its waterfall into a glade at sunset (lanterns
// lit). The whole way is drawn like the city: inked outlines, flat shading in two tones, layers of parallax. There the
// camera stops: its family
// waits on the right of the screen, it sees them, runs across behind the well to them, and they hug (from the moment
// it sees them it eats no more snacks: it is only with them).
//
// The world is drawn in shaders.hpp: what stands where along the way (world x) is fixed; the time of day and the
// weather follow the cub's progress (night, dawn as it rides out, morning in the fields, gold in the forest, sunset).
// The world's shader comes in three builds: the whole way, the city only, the countryside only (each leaves out the
// layers that cannot show, picked by the camera's place).
// The lines cleared set where the cub should be (the stages at 0, 7, 13, 19, 25, 32, 38 and 45 lines, the family at
// 50, within a song): the stairs at about 11-12 lines, the platform's end at about 19, the terminus at 25. The
// subway comes on its own timetable: a train glides in some 18 s into the scene and waits at the platform, its doors
// open, until the cub steps in (a missed train would leave a short song without its reunion). The ride runs on the
// train's clock (30 s door to door in a 50-line scene: some 5 s gathering speed, a run flat out, some 7 s braking);
// lines cleared meanwhile are banked, and the cub walks on with them from the terminus. A clear sends a few lights and
// a bamboo snack from the row, behind the well, to the cub: it catches the snack and munches it (paws at its mouth,
// cheeks puffing), then walks on; fed, it grows stronger on the way (straighter, bouncier, a little rounder). Between
// clears it lives: it breathes, blinks, looks around, looks back, sits, sighs, wipes a tear in the rain; later it
// hops, follows a butterfly. Every value of its pose eases toward its target.
// The pandas follow the owner's kawaii reference (drawn in shaders.hpp, measured against it: a sticker's flat fills in a
// thick dark outline, a round head wider than the slim body, round ears apart from the head, big round patches with big
// shiny eyes, ^ ^ in joy, an open smile, flat pink blush, black limbs and a black shoulder band, a little black tail;
// hearts by the head when waving for joy, sparkles when surprised): turned toward us (the front view) when it sits,
// sighs, wipes a tear or hops, in profile when it walks on or runs; the pose's turn moves it between the two.
//
// The music: the scene's light holds steady per section (brighter at a peak); in a peak the cub nods along every two
// beats and the fireflies wink on the beat grid. The drop lifts its spirits: a warm glow from the ground, lights
// rising around it, a few happy hops (it never walks further: a drop never skips a stage).
// The pieces are clean bamboo-segment cells, one solid color each; a cleared row shrinks away. Before a song the camera
// comes down out of the sky to the cub; after it the camera rises back into the sky.
#include "universes/lostpanda/lostpanda.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/lostpanda/shaders.hpp"

namespace {

// Fraunces Soft Italic (the song, the stage, the notices) and Nunito SemiBold (labels, numbers), SIL Open Font
// License, see OFL-Fraunces.txt and OFL-Nunito.txt.
const unsigned char FONT_ITAL[] = {
#include "universes/lostpanda/Fraunces-SoftItalic.ttf.inc"
};
const unsigned char FONT_SANS[] = {
#include "universes/lostpanda/Nunito-SemiBold.ttf.inc"
};

namespace shaders = lostpanda_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compile(const char* vs, const char* fs, const char* name, const char* defs = "") {
    std::string f(fs);
    const size_t at = f.find("//@TRAIN");
    if (at != std::string::npos) f.replace(at, 8, shaders::TRAIN_GLSL);
    f.insert(f.find('\n') + 1, std::string(defs) + shaders::COMMON_GLSL);
    std::string v(vs);
    v.insert(v.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(v.c_str(), f.c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned c) { return ugl::toLinear(hex(c)); }

// A variant: the seven pieces, the HUD's accent, the lights' warmth.
struct VariantDef {
    const char* name;
    unsigned piece[7]; // the blocks: bamboo, after the owner's mockup (beige, honey, pale, tan; sage, green, olive)
    unsigned accent, light;
};
const VariantDef VARIANTS[] = {
    {"LANTERN", {0xcaa972, 0x8c9a58, 0xd2aa58, 0x7d8c52, 0xd4b47a, 0x8c894d, 0xbea274}, 0xffc77a, 0xffc070},
    {"SUNNY", {0xcaa972, 0x8c9a58, 0xd2aa58, 0x7d8c52, 0xd4b47a, 0x8c894d, 0xbea274}, 0xa8d8ff, 0xffd890},
    {"BLOSSOM", {0xcaa972, 0x8c9a58, 0xd2aa58, 0x7d8c52, 0xd4b47a, 0x8c894d, 0xbea274}, 0xffb8cc, 0xffc8a0},
    {"JADE", {0xcaa972, 0x8c9a58, 0xd2aa58, 0x7d8c52, 0xd4b47a, 0x8c894d, 0xbea274}, 0xa8e8c0, 0xffd070},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));
int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }

// ---- The way home: one continuous world. The cub's place along it is its world x (board units).
constexpr int STAGES = 8;
// (lines on the 50-line scale: paceLines() rescales the real lines to the song's length, 50 for a 3:20 song; the
// subway's timetable and the ride stay on the clock)
const int LINES_AT[STAGES + 1] = {0, 7, 13, 19, 25, 32, 38, 45, 50};
constexpr double C_END = 1498.0;                     // the camera stops here (the glade); the family is on the right
const double WAY_AT[STAGES + 1] = {0, 46, 104, 128, 1327, 1381, 1431, 1476, C_END};
constexpr float GY = -7.4f;                          // the ground line on the screen (the cub's feet)
constexpr double GY0 = -7.4, PLAT_Y = -20.4;         // the street, the subway's platform (world y, as in the shaders)
constexpr double XB = 128.0, XA = 1327.0, DOOR = 30.0; // boarding, stepping out; the cub's door from the train's rear
constexpr float CLEAR_T = 0.38f;                     // a cleared row shrinks away in this long (real time)
// The subway's timetable (seconds): after a gap a train glides in, opens its doors and waits for the cub as long as
// it takes, then closes them and leaves with it.
constexpr float T_GAP = 8.f, T_ARRIVE_S = 10.f, T_DOOR_S = 1.2f, T_DWELL = 9.f, T_OUT_S = 10.f;
constexpr float T_RIDE_S = 30.f;                     // the ride, door to door (in a 50-line scene; shorter songs ride faster)
// The ride's course (0..1 of the way) over its time (0..1): a real journey, a long way fast. The train pulls out of
// the platform and gathers speed hard (the first RIDE_ACC of the time, some 5 s), runs flat out under the city (some
// 50 units a second: the street above, the tunnel's lamps and rings whip past), and brakes up the cutting into the
// terminus (the last RIDE_BRK); its speed eases in and out of each (a smoothstep), so it never jerks. The cruise is
// 1 / (1 - (RIDE_ACC + RIDE_BRK) / 2) of the mean speed.
constexpr double RIDE_ACC = 0.18, RIDE_BRK = 0.24;
double rideF(double u) {
    u = std::clamp(u, 0.0, 1.0);
    const double vc = 1.0 / (1.0 - 0.5 * (RIDE_ACC + RIDE_BRK));
    auto ramp = [](double t) { return t * t * t * (1.0 - 0.5 * t); };   // the way covered while the speed eases 0..1
    if (u < RIDE_ACC) return vc * RIDE_ACC * ramp(u / RIDE_ACC);
    if (u > 1.0 - RIDE_BRK) return 1.0 - vc * RIDE_BRK * ramp((1.0 - u) / RIDE_BRK);
    return vc * (0.5 * RIDE_ACC + u - RIDE_ACC);
}

double wayOf(double lines) {
    if (lines <= 0) return 0.0;
    for (int s = 0; s < STAGES; s++)
        if (lines < LINES_AT[s + 1]) {
            const double u = (lines - LINES_AT[s]) / (double)(LINES_AT[s + 1] - LINES_AT[s]);
            return WAY_AT[s] + (WAY_AT[s + 1] - WAY_AT[s]) * u;
        }
    return C_END;
}
double smd(double a, double b, double x) {
    const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
// Where the camera is on the ride (0..1 of the way over the ride's time, like rideF): it holds at the platform for a
// moment as the train pulls out, so the train visibly leaves ahead on the screen (some 10 units), then catches up with
// it within a few seconds and rides along with it at speed, the world streaming past; near the end it runs a little
// ahead into the terminus (a few units) and the train glides in under it. (Never backwards: the weights only ease the
// camera between the platform, the train and the terminus.)
double rideCamF(double u) {
    const double f = rideF(u);
    const double behind = 1.0 - smd(0.03, 0.15, u), ahead = 0.05 * smd(0.7, 0.9, u);
    return f - f * behind + (1.0 - f) * ahead;
}
double trackY(double x) {
    const double t = std::clamp((x - 1182.0) / 112.0, 0.0, 1.0);
    return PLAT_Y + (GY0 - PLAT_Y) * (0.95 * t + 0.05 * t * t * (3.0 - 2.0 * t));
}
constexpr double STAIR_X = 80.0, TREAD = 1.3, NSTEPS = 11.0;  // the subway's stairs (as in the shaders)
// the cub's feet (world y) along the way: the street, the stairs down, the platform, a hop into the train (its floor
// following the track up the cutting), a hop down onto the terminus's platform, the ground
double pathY(double x) {
    if (x < STAIR_X - 0.3) return GY0;
    if (x < STAIR_X + TREAD * NSTEPS) {
        // down the steps one by one: on a tread, then a little hop down to the next as it passes the edge
        const double n = (x - (STAIR_X - 0.3)) / TREAD;
        const double i = std::floor(n), fr = n - i;
        const double d = smd(0.0, 0.4, fr);
        const double level = std::min(NSTEPS, i + d);
        return GY0 - (GY0 - PLAT_Y) / NSTEPS * level + (level < NSTEPS ? 0.18 * std::sin(PI * d) : 0.0);
    }
    if (x < XB - 0.7) return PLAT_Y;
    if (x < XB) {
        const double u = (x - (XB - 0.7)) / 0.7;
        return PLAT_Y + (trackY(XB) + 0.5 - PLAT_Y) * smd(0.0, 1.0, u) + 0.9 * std::sin(PI * u);
    }
    if (x < XA) return trackY(x) + 0.5;
    if (x < XA + 0.9) {
        const double u = (x - XA) / 0.9;
        return trackY(XA) + 0.5 + (GY0 - trackY(XA) - 0.5) * smd(0.0, 1.0, u) + 0.7 * std::sin(PI * u);
    }
    return GY0;
}

// How each place looks (sRGB colors; the light on the pandas as plain multipliers).
struct StageDef {
    const char* name;
    unsigned skyTop, skyHor, fog, sunCol, veil;
    float sunX, sunY, sunR, sun, stars, clouds, rain, lamp;
    vec3 amb, key, rim;
    float keyX, keyY, mood, reflect, fireflies, butterfly;
};
const StageDef STAGE[STAGES] = {
    {"a rainy street", 0x0e0c2a, 0x33285a, 0x2a2350, 0xffb060, 0x08081a, 0, 0, 1, 0, 0.35f, 0, 0.7f, 1.0f,
     {0.30f, 0.34f, 0.46f}, {0.42f, 0.36f, 0.30f}, {0.55f, 0.42f, 0.28f}, 0.7f, 0.6f, 0.0f, 0.55f, 0.f, 0.f},
    {"the shopping street", 0x110c30, 0x3d2c66, 0x33285a, 0xff9a80, 0x0c0818, 0, 0, 1, 0, 0.5f, 0, 0.4f, 1.0f,
     {0.36f, 0.34f, 0.48f}, {0.48f, 0.38f, 0.44f}, {0.6f, 0.35f, 0.5f}, 0.7f, 0.6f, 0.08f, 0.45f, 0.f, 0.f},
    {"the subway", 0x110c30, 0x3d2c66, 0x33285a, 0xd8ffe8, 0x091010, 0, 0, 1, 0, 0.5f, 0, 0.3f, 1.0f,
     {0.50f, 0.56f, 0.55f}, {0.45f, 0.50f, 0.46f}, {0.45f, 0.55f, 0.5f}, 0.2f, 1.0f, 0.22f, 0.25f, 0.f, 0.f},
    {"the night train", 0x100e30, 0x342c60, 0x302a58, 0xffe0b0, 0x0c1016, 0, 0, 1, 0, 0.55f, 0, 0.15f, 1.0f,
     {0.56f, 0.56f, 0.54f}, {0.50f, 0.50f, 0.46f}, {0.4f, 0.4f, 0.36f}, 0.0f, 1.0f, 0.34f, 0.0f, 0.f, 0.f},
    {"the industrial district", 0x18204a, 0xdf9584, 0x7a6278, 0xffa868, 0x120e1c, 9.f, -6.8f, 1.0f, 0.45f, 0.3f, 0.25f, 0.0f, 0.8f,
     {0.46f, 0.42f, 0.52f}, {0.62f, 0.44f, 0.40f}, {0.9f, 0.55f, 0.42f}, 0.9f, 0.3f, 0.45f, 0.f, 0.f, 0.f},
    {"the fields", 0x5e9ad2, 0xf2e5c6, 0xcad6d4, 0xfff0c8, 0x162030, 14.f, 7.5f, 1.0f, 0.55f, 0.0f, 0.8f, 0.0f, 0.0f,
     {0.70f, 0.74f, 0.82f}, {0.72f, 0.68f, 0.58f}, {0.8f, 0.75f, 0.6f}, 0.6f, 0.8f, 0.66f, 0.f, 0.f, 1.f},
    {"the bamboo forest", 0x5c9c94, 0xeceebc, 0x86bcab, 0xffe49a, 0x10180e, 0, 0, 1, 0, 0.0f, 0.0f, 0.0f, 0.0f,
     {0.58f, 0.68f, 0.52f}, {0.74f, 0.68f, 0.48f}, {0.9f, 0.8f, 0.5f}, -0.6f, 0.8f, 0.82f, 0.f, 0.45f, 0.7f},
    {"the glade", 0x584c86, 0xffb070, 0xe2a084, 0xffc068, 0x1c1018, 16.f, -2.0f, 1.7f, 1.0f, 0.15f, 0.5f, 0.0f, 0.0f,
     {0.60f, 0.50f, 0.56f}, {0.90f, 0.64f, 0.44f}, {1.0f, 0.68f, 0.38f}, 1.0f, 0.25f, 1.0f, 0.f, 1.0f, 0.5f},
};

// A blend of two places, float by float.
struct Look {
    vec3 skyTop, skyHor, fog, sunCol, veil, amb, key, rim;
    float sunX, sunY, sunR, sun, stars, clouds, rain, lamp, keyX, keyY, mood, reflect, fireflies, butterfly;
};
static_assert(sizeof(Look) % sizeof(float) == 0, "a look is floats only");
Look lookOf(int s) {
    const StageDef& d = STAGE[std::clamp(s, 0, STAGES - 1)];
    Look l;
    l.skyTop = lin(d.skyTop); l.skyHor = lin(d.skyHor); l.fog = lin(d.fog); l.sunCol = lin(d.sunCol); l.veil = lin(d.veil);
    l.amb = d.amb; l.key = d.key; l.rim = d.rim;
    l.sunX = d.sunX; l.sunY = d.sunY; l.sunR = d.sunR; l.sun = d.sun; l.stars = d.stars; l.clouds = d.clouds;
    l.rain = d.rain; l.lamp = d.lamp; l.keyX = d.keyX; l.keyY = d.keyY; l.mood = d.mood; l.reflect = d.reflect;
    l.fireflies = d.fireflies; l.butterfly = d.butterfly;
    return l;
}
void mixLook(Look& a, const Look& b, float w) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < (int)(sizeof(Look) / sizeof(float)); i++) pa[i] += (pb[i] - pa[i]) * w;
}
// The look along the way: keyframes (a place's look at a world x), eased from one to the next: the weather and the
// time of day pass as the cub goes (night, the subway, the train, dawn as it rides out, morning, gold, sunset).
struct Key { double s; int stage; };
const Key KEYS[] = {{0, 0}, {40, 1}, {80, 1}, {92, 2}, {130, 3}, {1180, 3}, {1256, 4}, {1336, 4}, {1381, 5}, {1436, 6}, {1482, 7}};
constexpr int NKEYS = (int)(sizeof(KEYS) / sizeof(KEYS[0]));
Look lookAt(double s, int* stage = nullptr, float* blend = nullptr) {
    int i = 0;
    while (i < NKEYS - 1 && s >= KEYS[i + 1].s) i++;
    Look a = lookOf(KEYS[i].stage);
    float w = 0.f;
    if (i < NKEYS - 1) {
        w = (float)smd(KEYS[i].s, KEYS[i + 1].s, s);
        if (w > 0.f) mixLook(a, lookOf(KEYS[i + 1].stage), w);
    }
    if (stage) *stage = KEYS[i].stage;
    if (blend) *blend = w;
    return a;
}
// which part of the way (for the HUD and the cub's idle life)
int placeOf(double s) {
    if (s < 40.0) return 0;
    if (s < 84.0) return 1;
    if (s < XB - 0.5) return 2;
    if (s < XA + 0.5) return 3;
    if (s < 1376.0) return 4;
    if (s < 1428.0) return 5;
    if (s < 1476.0) return 6;
    return 7;
}

// Test hooks: REFALL_LOSTPANDA_LINES=N adds N lines to the way, REFALL_GROW_RATE=R adds R lines a second.
int extraLines(double t) {
    static const int n = std::getenv("REFALL_LOSTPANDA_LINES") ? std::atoi(std::getenv("REFALL_LOSTPANDA_LINES")) : 0;
    static const double rate = std::getenv("REFALL_GROW_RATE") ? std::atof(std::getenv("REFALL_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

// The idle actions.
enum Act { A_STAND, A_LOOK, A_LOOKUP, A_SIT, A_WIPE, A_STROLL, A_BUTTERFLY, A_HOP, A_SIGH, A_LOOKBACK, A_HUG, A_COUNT };

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool LostPandaUniverse::init(int width, int height) {
    progWorld_ = compile(ugl::FULLSCREEN_VS, shaders::WORLD_FS, "lostpanda world");
    // (lighter variants where only the city, or only the countryside, can show)
    progWorldCity_ = compile(ugl::FULLSCREEN_VS, shaders::WORLD_FS, "lostpanda world (city)", "#define ZONES\n#define CITY true\n#define COUNTRY false\n");
    progWorldCountry_ = compile(ugl::FULLSCREEN_VS, shaders::WORLD_FS, "lostpanda world (country)", "#define ZONES\n#define CITY false\n#define COUNTRY true\n");
    progNear_ = compile(ugl::FULLSCREEN_VS, shaders::WORLD_FS, "lostpanda near bamboo", "#define ZONES\n#define CITY false\n#define COUNTRY true\n#define NEAR_PASS\n");
    progFront_ = compile(ugl::FULLSCREEN_VS, shaders::FRONT_FS, "lostpanda front");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "lostpanda comp");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "lostpanda sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "lostpanda veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "lostpanda cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "lostpanda shape");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "lostpanda post");
    progPanda_ = compile(shaders::PANDA_VS, shaders::PANDA_FS, "lostpanda panda");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &quadVao_);
    glBindVertexArray(quadVao_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
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
    glBindVertexArray(0);

    // the family's own rhythms
    mom_.breath = 1.3f; mom_.blinkT = 3.1f; mom_.sit = 1.f; mom_.mood = 0.6f;
    dad_.breath = 2.6f; dad_.blinkT = 4.3f; dad_.sit = 1.f; dad_.mood = 0.6f;
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void LostPandaUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * (float)width_ / height_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    // the panda's spot: in the free band left of the HUD's left column
    // the free band left of the HUD's left column, the cub's size and the family's scale from its width
    const float bandL = viewC_.x - hw, bandW = layout::LEFT_COLUMN[0] - 0.4f - bandL;
    famS_ = std::clamp(bandW / 9.5f, 0.5f, 1.f);
    kidH_ = 4.2f * famS_;
    xP_ = bandL + 5.1f * famS_;
    // the reunion, in the free band right of the HUD's right column: the cub (it comes from the left), its mother,
    // its father furthest right
    const float bandR0 = layout::RIGHT_COLUMN[2] + 0.4f, bandRW = (viewC_.x + hw) - bandR0;
    kidEndX_ = bandR0 + 0.2f * bandRW;
    momEndX_ = bandR0 + 0.48f * bandRW;
    dadEndX_ = bandR0 + 0.77f * bandRW;
    enterX_ = 0.f;
    pEnd_ = C_END + (kidEndX_ - xP_);
    // the world at the full resolution (at most 1440 lines; every edge is antialiased in its shader)
    const int wh = std::max(180, std::min(1440, height_));
    const int ww = std::max(320, (int)std::lround((float)wh * width_ / height_));
    worldT_.create(ww, wh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

LostPandaUniverse::~LostPandaUniverse() {
    for (ugl::Target* t : {&worldT_, &sceneT_}) t->destroy();
    for (GLuint p : {progWorld_, progWorldCity_, progWorldCountry_, progFront_, progComp_, progSprite_, progVeil_, progCell_, progShape_, progPost_, progPanda_, progNear_})
        glDeleteProgram(p);
    for (GLuint v : {quadVao_, spriteVao_, cellVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, cellVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void LostPandaUniverse::setSong(const SongLook& t, float seconds) {
    (void)seconds;
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && !t.minor && t.brightness > 0.6f) v = 1;          // sunny for bright songs
        else if (t.hasSong && t.minor && t.brightness < 0.45f) v = 0;     // lanterns for dark ones
        else if (t.hasSong && t.airWeight > 0.6f) v = 2;                  // blossom for airy ones
        else {
            static const int BY_HUE[] = {0, 3, 1, 2, 0, 3};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x9A4DAull);
}

void LostPandaUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

void LostPandaUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

void LostPandaUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

vec2 LostPandaUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints LostPandaUniverse::hints() const {
    const Look l = lookAt(C_);
    TransitionHints h;
    h.background = lerp(l.skyHor, l.fog, 0.5f) * 0.6f;
    h.light = false;
    h.ink = lin(0xf6efe2);
    h.accent = lin(VARIANTS[wrapV(variant_)].accent);
    h.styles = TS_INK | TS_LUMA;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void LostPandaUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    const VariantDef& vd = VARIANTS[wrapV(variant_)];
    switch (ev.type) {
    case GameEvent::Spawn:
        for (int i = 0; i < 4; i++) pieceSeeds_[i] = 0.02f + 0.96f * rng_.uniform();
        break;
    case GameEvent::Lock:
        for (size_t i = 0; i < ev.cells.size() && i < 4; i++) {
            const auto& c = ev.cells[i];
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) seeds_[c.y][c.x] = pieceSeeds_[i];
        }
        break;
    case GameEvent::HardDrop: {
        // a little puff of dust pushed out from under the piece, at its lowest cells
        float yLo = 99.f;
        for (auto& c : ev.cells) yLo = std::min(yLo, layout::cellPos((float)c.x, (float)c.y).y);
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > yLo + 0.01f || motes_.size() > 300) continue;
            for (int i = 0; i < 2; i++) {
                Mote m;
                m.p = V(p.x + rng_.range(-0.4f, 0.4f), p.y - 0.5f);
                m.v = V(rng_.range(-0.8f, 0.8f), rng_.range(0.05f, 0.3f));
                m.life = rng_.range(0.5f, 0.8f);
                m.size = rng_.range(0.12f, 0.2f);
                m.col = lerp(lin(vd.piece[std::clamp(c.type, 0, 6)]), vec3(0.8f), 0.5f);
                m.alpha = 0.3f;
                motes_.push_back(m);
            }
        }
        break;
    }
    case GameEvent::Clear: {
        const int lines = std::max(1, ev.count);
        // each cleared cell shrinks away; each row sends lights of hope drifting to the panda, behind the well
        bool rows[Game::H] = {};
        for (auto& cl : ev.cells) {
            if (cl.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)cl.x, (float)cl.y);
            fading_.push_back({p.x, p.y, std::clamp(cl.type, 0, 6), 0.f, cl.y < Game::H && cl.x >= 0 && cl.x < Game::W ? seeds_[cl.y][cl.x] : 0.f});
            if (cl.y >= 0 && cl.y < Game::H) rows[cl.y] = true;
        }
        {   // the blocks' seeds follow their rows down
            float ns[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(ns[dst--], seeds_[y], sizeof(seeds_[y]));
            }
            std::memcpy(seeds_, ns, sizeof(seeds_));
        }
        const vec2 head = V(xP_ + (float)(P_ - C_), GY + (float)(pathY(P_) - GY0) - camY_ + kidH_ * 1.05f);
        int firstRow = -1;
        for (int y = 0; y < Game::H; y++) {
            if (!rows[y] || lights_.size() > 80) continue;
            if (firstRow < 0) firstRow = y;
            const float ry = layout::cellPos(0.f, (float)y).y;
            for (int i = 0; i < 2; i++) {
                Light l;
                l.a = V(rng_.range(-4.f, 4.f), ry);
                l.b = V(head.x + rng_.range(-1.6f, 1.6f), head.y + rng_.range(-0.4f, 2.2f));
                l.ctl = V(lerpf(l.a.x, l.b.x, 0.5f) + rng_.range(-1.f, 1.f), std::max(l.a.y, l.b.y) + rng_.range(2.f, 5.f));
                l.life = rng_.range(2.6f, 3.4f);
                l.age = -rng_.range(0.f, 0.35f);
                l.size = rng_.range(0.3f, 0.45f);
                l.seed = rng_.uniform();
                l.col = lin(vd.light);
                lights_.push_back(l);
            }
        }
        // red sparkles from the row to the cub (more for a quad; not once it has found its family: then it is only
        // with them)
        if (firstRow >= 0 && noticeT_ <= 0.f) {
            Light l;
            l.kind = lines >= 4 ? 2 : 1;
            l.a = V(rng_.range(-3.f, 3.f), layout::cellPos(0.f, (float)firstRow).y);
            l.ctl = V(lerpf(l.a.x, head.x, 0.5f), std::max(l.a.y, head.y) + 4.f);
            l.b = head;
            l.life = 1.5f;
            l.size = lines >= 4 ? 0.85f : 0.5f;
            l.seed = rng_.uniform();
            l.col = vec3(1.f);
            lights_.push_back(l);
        }
        if (lines >= 4) joy_ = std::max(joy_, 0.8f);
        lineDelay_ = 1.2f;
        break;
    }
    case GameEvent::TopOut: overT_ = 0.f; break;
    default: break;
    }
}

// =====================================================================================================
// The pandas' life
// =====================================================================================================

void LostPandaUniverse::chooseAct(float mood, int stage, bool home) {
    float w[A_COUNT] = {};
    const float sad = 1.f - mood;
    w[A_STAND] = 0.8f;
    w[A_LOOK] = 1.4f;
    w[A_LOOKUP] = 0.8f;
    w[A_SIT] = 0.6f + 1.0f * sad;
    w[A_WIPE] = mood < 0.5f ? 1.6f * sad * sad : 0.f;
    w[A_STROLL] = home ? 0.f : 0.7f;
    w[A_BUTTERFLY] = stage >= 5 && !home ? 1.6f : 0.f;
    w[A_HOP] = 1.4f * mood * mood;
    w[A_SIGH] = 0.9f * sad;
    w[A_LOOKBACK] = 0.8f * sad;
    w[A_HUG] = home ? 3.0f : 0.f;
    int a = rng_.weighted(w);
    if (a == act_ && a != A_HUG) a = rng_.weighted(w);
    act_ = a;
    actT_ = 0.f;
    static const float DUR[A_COUNT][2] = {{2.5f, 4.f}, {4.f, 7.f}, {3.f, 5.f}, {6.f, 10.f}, {4.f, 5.f}, {3.f, 4.5f},
                                          {6.f, 9.f}, {2.4f, 3.f}, {3.f, 4.f}, {3.f, 5.f}, {6.f, 10.f}};
    actDur_ = rng_.range(DUR[a][0], DUR[a][1]);
    if (a == A_STROLL) stroll_ = std::min(P_ + rng_.range(0.6f, 1.1f), lineTarget_ + 1.3);
}

void LostPandaUniverse::updatePose(Pose& p, float dt, const Pose& t, float rate) {
    const float turned = p.turn;
    // facing the other way: at once, mirrored (never turning round through the front view); a slight turn the other
    // way goes only as far as facing us
    const float side = p.turn < 0.f ? -1.f : 1.f;
    if (t.turn * side < -0.2f) p.turn = -p.turn;
    p.turn = approach(p.turn, t.turn * (p.turn < 0.f ? -1.f : 1.f) < 0.f ? 0.f : t.turn, rate * 0.7f, dt);
    // mirrored, the near ear becomes the far one: each keeps its spring
    if ((turned < 0.f) != (p.turn < 0.f)) {
        std::swap(p.earN, p.earF);
        std::swap(p.earVN, p.earVF);
    }
    p.tilt = approach(p.tilt, t.tilt, rate, dt);
    p.lookX = approach(p.lookX, t.lookX, rate * 1.6f, dt);
    p.lookY = approach(p.lookY, t.lookY, rate * 1.6f, dt);
    p.sit = approach(p.sit, t.sit, rate * 0.6f, dt);
    p.wipe = approach(p.wipe, t.wipe, rate * 0.9f, dt);
    p.tear = approach(p.tear, t.tear, rate * 0.5f, dt);
    p.hop = approach(p.hop, t.hop, 16.f, dt);
    p.lean = approach(p.lean, t.lean, rate, dt);
    p.arms = approach(p.arms, t.arms, rate * 0.9f, dt);
    p.joyEyes = approach(p.joyEyes, t.joyEyes, rate * 1.2f, dt);
    p.hug = approach(p.hug, t.hug, rate * 0.7f, dt);
    p.munch = approach(p.munch, t.munch, 7.f, dt);
    p.surprise = approach(p.surprise, t.surprise, 5.f, dt);
    p.chew += dt * 11.f * p.munch;
    p.mood = approach(p.mood, t.mood, 0.5f, dt);
    // breath, blinks, the ears on their springs
    p.breath += dt * (TAU / 3.6f) * (1.f + 0.4f * p.walk);
    p.wind = approach(p.wind, t.wind, 1.f, dt);
    p.blinkAge += dt;
    p.blinkT -= dt;
    if (p.blinkT <= 0.f) {
        p.blinkAge = 0.f;
        p.blinkT = rng_.range(2.2f, 5.5f);
        if (rng_.chance(0.2f)) p.blinkT = 0.35f; // a double blink
    }
    p.blink = p.blinkAge < 0.18f ? std::sin(p.blinkAge / 0.18f * PI) : 0.f;
    const float target = 0.08f * std::sin(p.phase * 2.f) * p.walk;
    for (int i = 0; i < 2; i++) {
        float& e = i == 0 ? p.earN : p.earF;
        float& v = i == 0 ? p.earVN : p.earVF;
        v += (60.f * (target - e) - 5.f * v) * dt;
        e += v * dt;
    }
    if (rng_.chance(dt * 0.18f)) (rng_.chance(0.5f) ? p.earVN : p.earVF) += rng_.range(-2.5f, 2.5f);
}

void LostPandaUniverse::updateKid(float dt, float stageMood, int stage, bool travelling, float speed) {
    Pose t = kid_;
    const bool home = P_ >= pEnd_ - 0.05;
    const float mood = std::clamp(stageMood + 0.35f * joy_ + 0.5f * reunion_ - (overT_ >= 0.f ? 0.4f : 0.f), 0.f, 1.f);
    t.mood = mood;
    t.turn = 0.55f; t.tilt = 0.f; t.lookX = 0.6f; t.lookY = -0.3f * (1.f - mood);
    t.sit = 0.f; t.wipe = 0.f; t.tear = 0.f; t.hop = 0.f; t.lean = 0.f; t.arms = 0.f; t.joyEyes = 0.f; t.hug = 0.f;
    t.surprise = 0.f;
    t.wind = 0.3f + 0.5f * kid_.walk;
    actT_ += dt;
    // the lights of a clear coming: watch them (toward the well, up)
    float watch = 0.f;
    for (const Light& l : lights_) if (l.age > 0.f && l.age < l.life * 0.8f) watch = 1.f;
    if (overT_ >= 0.f) {
        t.sit = 1.f; t.tilt = -0.2f; t.turn = 0.15f; t.lookY = -1.f;
    } else if (travelling) {
        // walking on: looking ahead; running in a quad or a joyful mood
        t.turn = 0.7f; t.tilt = -0.04f * (1.f - mood); t.lookX = 1.f; t.lookY = -0.1f;
        t.lean = std::min(1.f, speed / 3.f);
        if (speed > 2.6f) t.hop = std::fabs(std::sin(kid_.phase)) * 0.035f * std::min(1.f, (speed - 2.6f) / 1.f);
        actT_ = 0.f;
        act_ = A_STAND;
        actDur_ = rng_.range(1.f, 2.5f);
    } else {
        if (actT_ > actDur_) chooseAct(mood, stage, home);
        const float a = actT_;
        const float in = smoothstepf(0.f, 0.6f, a) * (1.f - smoothstepf(actDur_ - 0.8f, actDur_, a));
        switch (act_) {
        case A_LOOK:
            t.turn = 0.25f + 0.75f * std::sin(a * 0.75f);
            t.lookX = t.turn > 0.f ? 1.f : -1.f;
            t.tilt = 0.05f * std::sin(a * 0.5f);
            break;
        case A_LOOKUP:
            t.tilt = 0.26f * in; t.lookY = 1.f; t.turn = 0.35f;
            break;
        case A_SIT:
            t.sit = smoothstepf(0.2f, 0.8f, a) * (1.f - smoothstepf(actDur_ - 1.f, actDur_ - 0.2f, a));
            t.turn = 0.2f + 0.2f * std::sin(a * 0.4f); t.tilt = -0.06f * (1.f - mood); t.lookY = -0.6f * (1.f - mood);
            break;
        case A_WIPE:
            t.tear = (a > 0.2f && a < 2.4f) ? 1.f : 0.f;
            t.wipe = (a > 1.6f && a < 3.4f) ? 1.f : 0.f;
            if (a > 2.3f) t.tear = 0.f;
            t.tilt = -0.12f; t.turn = 0.15f; t.lookY = -1.f;
            break;
        case A_STROLL:
            t.turn = 0.7f; t.lookX = 1.f;
            break;
        case A_BUTTERFLY: break; // the head follows the butterfly (below)
        case A_HOP: {
            const float k = a < 2.0f ? std::fabs(std::sin(a * PI / 0.5f)) : 0.f;
            t.hop = k * 0.075f; t.arms = 0.7f * in; t.joyEyes = in; t.turn = 0.3f;
            break;
        }
        case A_SIGH:
            t.tilt = -0.2f * in; t.lookY = -1.f; t.turn = 0.2f; t.lean = -0.3f * in;
            break;
        case A_LOOKBACK: // (facing us, its eyes back the way it came: it never turns its back on where it goes)
            t.turn = 0.55f * (1.f - in); t.lookX = -1.f; t.tilt = 0.05f;
            break;
        case A_HUG: {
            t.joyEyes = in; t.turn = 0.5f; t.tilt = 0.12f * in; t.lean = 0.8f * in;   // leaning into its mother
            break;
        }
        default: break;
        }
        if (watch > 0.f && act_ != A_HUG) { t.turn = 0.75f; t.lookX = 1.f; t.lookY = 0.8f; t.tilt = 0.18f; t.sit = 0.f; t.wipe = 0.f; }
    }
    // the butterfly: looked at while it flutters near
    if (flyA_ > 0.3f && (act_ == A_BUTTERFLY || act_ == A_LOOK || act_ == A_STAND) && !travelling && overT_ < 0.f) {
        const vec2 head = V(xP_ + (float)(P_ - C_), GY + kidH_ * 0.75f);
        const float dx = fly_.x - head.x, dy = fly_.y - head.y;
        t.turn = std::clamp(dx * 0.6f, 0.f, 0.9f); // (on its left: watched facing us)
        t.lookX = dx > 0.f ? 1.f : -1.f;
        t.lookY = std::clamp(dy * 0.8f, -1.f, 1.f);
        t.tilt = std::clamp(dy * 0.08f, -0.1f, 0.25f);
        if (act_ == A_BUTTERFLY) t.joyEyes = 0.f;
    }
    // the end: it sees its family on the right, stops, then runs to them; there, the hug
    if (noticeT_ > 0.f && overT_ < 0.f) {
        if (!home) {
            t.turn = 0.85f; t.lookX = 1.f; t.lookY = 0.1f; t.sit = 0.f; t.wipe = 0.f; t.tear = 0.f;
            t.tilt = noticeT_ < 1.8f ? 0.1f : 0.f;
            t.arms = noticeT_ < 1.8f ? 0.f : 0.55f;
            t.joyEyes = noticeT_ < 1.8f ? 0.f : 0.35f;
            t.surprise = noticeT_ < 1.8f ? 1.f : 0.f;   // there they are!
            actT_ = 0.f; act_ = A_HUG; actDur_ = 9.f;
        } else if (arriveT_ < 1.f) {
            act_ = A_HUG; actT_ = std::max(actT_, 0.7f); actDur_ = std::max(actDur_, 9.f);
        }
    }
    // a missed train: it watches it pull out into the tunnel, then its head drops, a tear, it sits to wait
    if (missedT_ > 0.f && overT_ < 0.f && !travelling) {
        const float m = missedT_;
        t.turn = m < 4.f ? 0.85f : 0.3f; t.lookX = 1.f; t.lookY = m < 4.f ? 0.f : -0.8f;
        t.tilt = m < 1.5f ? 0.08f : -0.16f;
        t.tear = (m > 2.5f && m < 7.f) ? 1.f : 0.f;
        t.wipe = (m > 6.f && m < 8.f) ? 1.f : 0.f;
        t.sit = (m > 4.f && m < 11.f) ? 1.f : 0.f;
        t.mood = std::min(t.mood, 0.15f);
        t.joyEyes = 0.f; t.hop = 0.f; t.arms = 0.f;
        actT_ = 0.f; act_ = A_STAND; actDur_ = 2.f;
    }
    // munching a snack: paws at its mouth, happy
    t.munch = munchT_ > 0.f ? 1.f : 0.f;
    if (munchT_ > 0.f) { t.joyEyes = std::max(t.joyEyes, 0.45f); t.wipe = 0.f; t.tear = 0.f; t.arms = 0.f; }
    // a drop: the spirits lift, a look up, a few hops
    if (joy_ > 0.4f && !travelling && act_ != A_HUG && overT_ < 0.f) {
        t.tilt = std::max(t.tilt, 0.15f * joy_);
        t.lookY = 0.7f;
        t.sit = 0.f; t.wipe = 0.f; t.tear = 0.f;
    }
    t.turn = std::max(t.turn, 0.f); // the cub always faces us or the way home, never back
    updatePose(kid_, dt, t, 3.f);
}

// =====================================================================================================
// Drawing helpers
// =====================================================================================================

void LostPandaUniverse::cell(float x, float y, float half, const vec3& tint, float alpha, int mode, float erase, float glow,
                             float seed, int nb) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)nb}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, glow, seed}});
}

float LostPandaUniverse::rideS() const { return T_RIDE_S * std::min(1.f, 1.f / paceScale()); }

void LostPandaUniverse::buildBoard(const Game& g) {
    const VariantDef& vd = VARIANTS[wrapV(variant_)];
    auto tintOf = [&](int t) { return lin(vd.piece[std::clamp(t, 0, 6)]); };
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
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            float& sd = seeds_[y][x];
            if (sd <= 0.f) sd = 0.02f + 0.96f * std::fmod(std::sin(x * 12.9898f + y * 78.233f) * 43758.5453f + 1e5f, 1.f);
            // (the blocks touching it: its grooves in their shade; a row still closing up after a clear touches none)
            auto at = [&](int xx, int yy) {
                return xx >= 0 && xx < Game::W && yy >= Game::HIDDEN && yy < Game::H && g.cell(xx, yy).type >= 0 &&
                       g.rowOffset(yy) == g.rowOffset(y);
            };
            const int nb = (at(x, y - 1) ? 1 : 0) | (at(x, y + 1) ? 2 : 0) | (at(x - 1, y) ? 4 : 0) | (at(x + 1, y) ? 8 : 0);
            cell(p.x, p.y, 0.5f, tintOf(c.type) * darken, 1.f, 0, 0.f, 0.f, sd, nb);
        }
    for (const Fading& f : fading_) cell(f.x, f.y, 0.5f, tintOf(f.type), 1.f, 0, saturate(f.t / CLEAR_T), 0.f, f.seed);
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
                cell(w.x, w.y, 0.5f, tintOf(p.type), 0.6f, 1, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            const auto& c = cells[i];
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, tintOf(p.type), 1.f, 0, 0.f, 0.f, pieceSeeds_[i], pieceMask(cells, i));
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
            cell(px, py, 0.5f * sc, tintOf(type), alpha, 0, 0.f, 0.f, 0.1f + 0.11f * type + 0.2f * i, pieceMask(cells, i));
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
    // the cubes drawn from the far ones to the near ones as the eye sees them (it looks down on them from over the
    // well's middle): the ghost first, then the lowest rows first, and in a row the outer columns first, so a block
    // hides the top of the one under it and the inner side of the one beside it
    std::stable_sort(cells_.begin(), cells_.end(), [](const CellInst& a, const CellInst& b) {
        if (a.c[0] != b.c[0]) return a.c[0] > b.c[0];
        if (std::fabs(a.a[1] - b.a[1]) > 0.01f) return a.a[1] < b.a[1];
        return std::fabs(a.a[0]) > std::fabs(b.a[0]);
    });
}

void LostPandaUniverse::sprite(std::vector<SpriteInst>& out, vec2 p, float size, const vec3& col, float alpha, int mode,
                               float param, float param2) {
    if (alpha <= 0.004f || size <= 0.f) return;
    out.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {param, param2, 0.f, 0.f}});
}

void LostPandaUniverse::drawSprites(const std::vector<SpriteInst>& v, bool additive) {
    if (v.empty()) return;
    (void)additive;
    glUseProgram(progSprite_);
    setMat(progSprite_, "uVP", vp_);
    set1f(progSprite_, "uPxPerUnit", pxPerUnit_);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(SpriteInst), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
}

// =====================================================================================================
// The HUD
// =====================================================================================================

void LostPandaUniverse::drawHud(const std::vector<HudRect>& panels) {
    if (panels.empty()) return;
    const Look lk = lookAt(C_);
    std::vector<float> v;
    for (const HudRect& r : panels) {
        const vec3 c = r.custom ? ugl::toLinear(r.color) : r.accent ? lin(VARIANTS[wrapV(variant_)].accent) : lk.veil;
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
// soft italics for the song, the variant, the notices, and the place on the way home after the universe's name;
// each line on a soft halo of the night's veil. The inks are the scene's (linear), toned as the post tones the
// scene: dimmed, their highlights rolled off, to sRGB.
void LostPandaUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* sans = text::font(FONT_SANS);
    text::Font* ital = text::font(FONT_ITAL);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount), dim = 1.f - dimT_ * 0.5f;
    auto tone = [&](const vec3& c0) {
        vec3 c = c0 * dim;
        for (float* v : {&c.x, &c.y, &c.z}) *v /= 1.f + std::max(*v - 0.8f, 0.f) * 0.8f;
        return ugl::toSrgb(c);
    };
    auto inkOf = [&](const vec3& c) { return tone(lerp(c, bridge_.ink, b)); };
    const vec3 cream = lin(0xf6efe2), acc = lin(VARIANTS[wrapV(variant_)].accent);
    const vec3 ink = inkOf(cream), accent = inkOf(acc);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    float halo = 0.75f; // the halo's amount
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        // the name in spaced capitals, then the variant and the place in italics, a little higher
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
        text::Span& st = out.add(stageName_, ital, vsz, ink, 0.f, a * 0.72f);
        st.gap = (gap * 1.3f - (var.empty() ? tr : 0.f)) / vsz;
        st.dy = dy;
        break;
    }
    case HUD_SECTION: put(sans, h.text, base * 0.9f, base * 0.45f, ink, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(ital, h.text, base * 1.65f, 0.f, ink, h.alpha * 1.3f, -base * 0.35f); break;
    case HUD_INFO: put(sans, h.text, base * 0.66f, base * 0.12f, ink, saturate(alpha * 1.3f) * 0.8f); break;
    case HUD_LABEL: put(sans, h.text, base * 0.78f, base * 0.42f, inkOf(lerp(cream, acc, 0.5f)), saturate(alpha * 1.2f) * 0.92f); break;
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
    l.p0 = vec4(tone(lerp(lookAt(C_).veil * 0.8f, bridge_.color, b)), halo);
    out.lettering = &l;
    out.alpha = saturate(fade_);
}

// =====================================================================================================
// The pandas
// =====================================================================================================

void LostPandaUniverse::drawPanda(const Pose& p, vec2 feet, float h, float face, float adult, float alpha, float haze,
                                  bool reflect, int part) {
    if (alpha <= 0.003f) return;
    const float x0 = feet.x - 0.75f * h, x1 = feet.x + 0.75f * h;
    if (x1 < viewC_.x - viewH_.x || x0 > viewC_.x + viewH_.x) return;
    const Look l = lookAt(C_ + (feet.x - xP_));
    glUseProgram(progPanda_);
    setMat(progPanda_, "uVP", vp_);
    set4f(progPanda_, "uRect", x0, feet.y - (reflect ? 0.75f * h : 0.08f * h), x1, feet.y + 1.35f * h);
    set2f(progPanda_, "uPos", feet.x, feet.y);
    set1f(progPanda_, "uH", h);
    set1f(progPanda_, "uFace", face);
    set1f(progPanda_, "uPx", 1.f / pxPerUnit_);
    set1f(progPanda_, "uAdult", adult);
    set1f(progPanda_, "uTime", time_);
    glUniform1i(ugl::U(progPanda_, "uPart"), part);
    set1f(progPanda_, "uWalk", p.walk);
    set1f(progPanda_, "uPhase", p.phase);
    set1f(progPanda_, "uMood", p.mood);
    set1f(progPanda_, "uBreath", p.breath);
    set1f(progPanda_, "uTurn", p.turn);
    set1f(progPanda_, "uTilt", p.tilt);
    set1f(progPanda_, "uBlink", p.blink);
    set1f(progPanda_, "uSit", p.sit);
    set1f(progPanda_, "uWipe", p.wipe);
    set1f(progPanda_, "uTear", p.tear);
    set1f(progPanda_, "uHop", p.hop);
    set1f(progPanda_, "uLean", p.lean);
    set1f(progPanda_, "uArms", p.arms);
    set1f(progPanda_, "uJoyEyes", p.joyEyes);
    set1f(progPanda_, "uHug", p.hug);
    set1f(progPanda_, "uMunch", p.munch);
    set1f(progPanda_, "uChew", p.chew);
    set1f(progPanda_, "uSurprise", p.surprise);
    set1f(progPanda_, "uStrength", adult > 0.5f ? 1.f : (sheet_ ? 0.f : strength_));
    set1f(progPanda_, "uRoll", adult > 0.5f || sheet_ ? 0.f : kidRoll_);
    set2f(progPanda_, "uLook", p.lookX, p.lookY);
    set2f(progPanda_, "uEars", p.earN, p.earF);
    const float night = 1.f - 0.5f * smoothstepf(0.f, 1.f, over_);
    set3f(progPanda_, "uAmb", sheet_ ? vec3(1.f) : l.amb * night);
    set3f(progPanda_, "uKey", sheet_ ? vec3(0.f) : l.key * night * (1.f + 0.25f * dropAmt_));
    set3f(progPanda_, "uRimC", l.rim * night);
    const float kl = std::sqrt(l.keyX * l.keyX + l.keyY * l.keyY) + 1e-4f;
    set2f(progPanda_, "uKeyDir", l.keyX / kl, l.keyY / kl);
    set1f(progPanda_, "uReflect", reflect ? l.reflect : 0.f);
    set1f(progPanda_, "uShadow", 0.6f);
    set1f(progPanda_, "uAlpha", alpha);
    set1f(progPanda_, "uHaze", haze);
    set3f(progPanda_, "uHazeC", l.fog);
    glBindVertexArray(quadVao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// the test hook REFALL_LOSTPANDA_SHEET: the pandas alone, big, posed
void LostPandaUniverse::drawSheet(int which) {
    sceneT_.bind();
    glClearColor(0.79f, 0.745f, 0.665f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    sheet_ = true;
    const float hx = viewH_.x, hy = viewH_.y, cx = viewC_.x, cy = viewC_.y;
    Pose b;
    b.mood = 1.f; b.turn = 0.f; b.lookX = 0.f; b.lookY = 0.f; b.breath = 0.f;
    if (which == 1) {
        // as the reference: two columns, facing us above (happy, surprised), in profile below
        const float h = 0.86f * hy;
        Pose p = b;
        drawPanda(p, V(cx - 0.5f * hx, cy + 0.06f * hy), h, 1.f, 0.f, 1.f, 0.f, false, 0);
        p.surprise = 1.f;
        drawPanda(p, V(cx + 0.5f * hx, cy + 0.06f * hy), h, 1.f, 0.f, 1.f, 0.f, false, 0);
        p = b; p.turn = 0.7f;
        drawPanda(p, V(cx - 0.5f * hx, cy - 0.94f * hy), h, 1.f, 0.f, 1.f, 0.f, false, 0);
        drawPanda(p, V(cx + 0.5f * hx, cy - 0.94f * hy), h, 1.f, 0.f, 1.f, 0.f, false, 0);
    } else {
        // its other poses: sad sitting, wiping a tear, waving for joy, munching (facing, in profile), walking, the
        // hug, a blink, the parents (sitting, waving)
        const float h = 0.82f * hy;
        Pose ps[10];
        for (Pose& p : ps) p = b;
        ps[0].mood = 0.1f; ps[0].sit = 1.f; ps[0].turn = 0.2f; ps[0].lookY = -0.6f; ps[0].tilt = -0.06f;
        ps[1].mood = 0.1f; ps[1].wipe = 1.f; ps[1].tear = 0.7f; ps[1].turn = 0.2f;
        ps[2].arms = 0.7f; ps[2].joyEyes = 1.f; ps[2].turn = 0.3f; ps[2].hop = 0.04f;
        ps[3].munch = 1.f; ps[3].chew = 1.f; ps[3].joyEyes = 0.45f; ps[3].turn = 0.15f;
        ps[4].munch = 1.f; ps[4].chew = 1.f; ps[4].joyEyes = 0.45f; ps[4].turn = 0.7f;
        ps[5].mood = 0.5f; ps[5].walk = 1.f; ps[5].phase = 1.3f; ps[5].turn = 0.7f; ps[5].lookX = 1.f;
        ps[6].hug = 1.f; ps[6].joyEyes = 1.f; ps[6].turn = 0.5f; ps[6].lean = 0.8f; ps[6].tilt = 0.12f;
        ps[7].blink = 1.f; ps[7].mood = 0.6f; ps[7].turn = 0.1f;
        ps[8].mood = 0.6f; ps[8].sit = 1.f; ps[8].turn = 0.3f;
        ps[9].arms = 1.f; ps[9].joyEyes = 1.f; ps[9].turn = 0.5f;
        for (int i = 0; i < 10; i++) {
            const float x = cx + hx * (-0.8f + 0.4f * (float)(i % 5)), y = cy + (i < 5 ? 0.08f : -0.92f) * hy;
            drawPanda(ps[i], V(x, y), i >= 8 ? h * 1.1f : h, i == 9 ? -1.f : 1.f, i >= 8 ? 1.f : 0.f, 1.f, 0.f, false, 0);
        }
    }
    sheet_ = false;
}

// =====================================================================================================
// Frame
// =====================================================================================================

void LostPandaUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 2.5f) : 0.f, overT_ >= 0.f ? 3.f : 0.8f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, slow_);
    time_ += wdt;
    const VariantDef& vd = VARIANTS[wrapV(variant_)];

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
        // the drop: the spirits lift; lights rise all around the cub
        joy_ = 1.f;
        if (act_ != A_HUG) { act_ = A_HOP; actT_ = 0.f; actDur_ = 2.8f; }
        const vec2 at = V(xP_ + (float)(P_ - C_), GY);
        for (int i = 0; i < 16 && lights_.size() < 120; i++) {
            Light l;
            l.a = V(at.x + rng_.range(-4.f, 4.f), GY + rng_.range(-1.5f, 0.5f));
            l.b = V(l.a.x + rng_.range(-2.f, 2.f), GY + rng_.range(6.f, 13.f));
            l.ctl = V(at.x + rng_.range(-3.f, 3.f), lerpf(l.a.y, l.b.y, 0.5f));
            l.life = rng_.range(4.f, 6.f);
            l.age = -rng_.range(0.f, 1.2f);
            l.size = rng_.range(0.3f, 0.5f);
            l.seed = rng_.uniform();
            l.col = lin(vd.light);
            lights_.push_back(l);
        }
    }
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 2.5f : 0.6f, dt); // (the drop's colours come in on its first bars)
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    joy_ = approach(joy_, 0.f, 0.22f, dt);
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
    // every two beats, a soft bump (a peak's nod, the fireflies winking)
    const float half = (float)std::fmod(beat * 0.5, 1.0);
    const float nod = std::exp(-half * 5.f) * wp;

    // ---- The way: the game's lines -> where the cub should be.
    bool travelling = false;
    float speed = 0.f;
    trainClock_ += wdt;
    {
        // (on the 50-line scale: the song's pace, paceLines; a break: the way as it was)
        const float lines = f.bare ? lines_ : paceLines(game, (float)extraLines(time_));
        if (linesShown_ >= 0 && lines < lines_ && !game.over()) {
            // a new game: back to the rainy street (the world dimmed by the game over hides the jump)
            placed_ = false;
            lights_.clear();
            kid_.sit = 0.f;
        }
        lines_ = lines;
        linesShown_ = lines;
        if (!placed_) {
            lineTarget_ = lines >= LINES_AT[STAGES] ? pEnd_ : wayOf(lines);
            P_ = lineTarget_;
            C_ = std::min(P_, C_END);
            camY_ = (float)(pathY(P_) - GY0);
            vel_ = 0.0;
            stroll_ = creep_ = P_;
            linesWalk_ = lines;
            noticeT_ = arriveT_ = 0.f;
            // the train where it should be: gone, or the cub riding it, or its timetable starting afresh
            boardT_ = leaveT_ = trainT_ = missedT_ = 0.f;
            if (P_ >= XA - 0.05) { train_ = T_GONE; trainX_ = -1000.0; door_ = 0.f; P_ = std::max(P_, XA + 1.2); }
            else if (P_ > XB - 1.0) {
                // riding: find where the ride is from where the lines put the cub
                const double want = (P_ - XB) / (XA - XB);
                double lo = 0.0, hi = 1.0;
                for (int i = 0; i < 40; i++) { const double m = 0.5 * (lo + hi); (rideF(m) < want ? lo : hi) = m; }
                train_ = T_RIDE; rideT_ = (float)(lo * rideS()); door_ = 0.f;
                trainX_ = XB - DOOR + (XA - XB) * rideF(lo);
                P_ = trainX_ + DOOR;
            } else { train_ = T_AWAY; trainX_ = XB - DOOR - 160.0; door_ = 0.f; }
            C_ = train_ == T_RIDE ? XB + (XA - XB) * rideCamF(rideT_ / rideS()) : std::min(P_, C_END);
            camY_ = (float)(pathY(P_) - GY0);
            placed_ = true;
        }
        lineDelay_ -= wdt;
        if (lineDelay_ <= 0.f) linesWalk_ = lines_;
        lineTarget_ = linesWalk_ >= LINES_AT[STAGES] ? pEnd_ : wayOf(linesWalk_);
        // where the lines would take the cub; the train decides the rest: it waits for a train at the platform's end,
        // steps in while the doors are open, rides (the train's clock, not the lines), steps out at the terminus;
        // lines cleared meanwhile are banked: it walks on with them afterwards
        const bool riding = train_ == T_RIDE;
        double target = lineTarget_;
        double cap = pEnd_;
        const bool onFoot = P_ < 78.0 || (P_ > XA + 2.0 && P_ < C_END - 2.0);
        if (onFoot) target = std::max(target, std::min(stroll_, lineTarget_ + 1.3));
        if (P_ < XB - 0.03) {
            // before the train: as far as the platform's end, and in through the doors if they are open
            cap = XB - 1.0;
            if (train_ == T_WAIT && door_ > 0.95f && trainT_ < T_DOOR_S + T_DWELL && lineTarget_ >= XB - 1.01) { target = XB; cap = XB; }
            if (P_ > XB - 0.9) { target = XB; cap = XB; }                     // (mid-step: it finishes the step)
        } else if (P_ < XA - 0.05) {
            target = cap = P_;                                                // aboard: the train carries it
        } else {
            // the terminus: out onto the platform when the doors open, then on with the banked lines
            target = std::max(target, XA + 1.2);
            if (train_ == T_TERM && door_ < 0.95f && P_ < XA + 0.5) cap = XA;
        }
        // the end: the cub sees its family, stops a moment, then runs to them
        if (linesWalk_ >= LINES_AT[STAGES] && P_ >= C_END - 0.1) noticeT_ += wdt;
        if (noticeT_ < 1.8f) cap = std::min(cap, C_END);
        target = std::min(target, cap);
        if (overT_ >= 0.f && !riding) target = std::min(target, P_);
        const double dist = target - P_;
        const float mood = kid_.mood;
        const bool lineWalk = dist > 0.05;
        // a calm pace: a stroll, a walk, a brisk walk when it has a long way to make up (never a dash)
        // the further behind its lines, the faster it goes (a brisk walk, then a trot): the way must always be
        // feasible within the song, however fast the lines come
        double vmax = lineWalk ? std::min(6.5, lerpf(1.5f, 2.4f, mood) + std::max(0.0, dist - 6.0) * 0.12) : 0.45;
        if (P_ > 79.0 && P_ < 95.0) vmax = std::min(vmax, 1.2);                 // careful on the stairs
        if (P_ > XB - 1.0 && P_ < XB + 0.1) vmax = std::min(vmax, 1.0);        // stepping in
        if (P_ > XA - 0.1 && P_ < XA + 1.0) vmax = std::min(vmax, 1.0);        // stepping out
        if (noticeT_ >= 1.8f && P_ < pEnd_) vmax = 3.4;                        // running to them
        { // (testing: with REFALL_GROW_RATE the cub keeps up with the growing lines)
            static const double grow = std::getenv("REFALL_GROW_RATE") ? std::atof(std::getenv("REFALL_GROW_RATE")) : 0.0;
            vmax += grow * 2.5;
        }
        if (!riding) {
            const double want = std::clamp(dist * 1.1, 0.0, vmax);
            vel_ += (want - vel_) * (1.0 - std::exp(-2.6 * wdt));
            if (dist < 0.002 && vel_ < 0.02) vel_ = 0.0;
            const double before = P_;
            P_ += vel_ * wdt;
            if (P_ > cap) { P_ = std::max(before, cap); vel_ = 0.0; }   // never past a gate
        }
        // the train, on its own clock
        trainT_ += wdt;
        switch (train_) {
        case T_AWAY:
            trainX_ = XB - DOOR - 160.0;
            door_ = 0.f;
            if (trainT_ >= T_GAP) { train_ = T_ARRIVE; trainT_ = 0.f; }
            break;
        case T_ARRIVE: {
            const double u = std::min(1.0, trainT_ / (double)T_ARRIVE_S);
            trainX_ = XB - DOOR - 130.0 * (1.0 - u) * (1.0 - u);
            if (u >= 1.0) { train_ = T_WAIT; trainT_ = 0.f; }
            break;
        }
        case T_WAIT: {
            trainX_ = XB - DOOR;
            // the doors stay open until the cub is aboard: the train never leaves without it
            if (P_ < XB - 0.03 && trainT_ > T_DOOR_S + T_DWELL - 0.5f) trainT_ = T_DOOR_S + T_DWELL - 0.5f;
            const bool closing = trainT_ > T_DOOR_S + T_DWELL;
            door_ = std::clamp(door_ + wdt / T_DOOR_S * (closing ? -1.f : 1.f), 0.f, 1.f);
            if (closing && door_ <= 0.f) {
                if (P_ >= XB - 0.03) { train_ = T_RIDE; rideT_ = 0.f; }
                else {
                    train_ = T_OUT; trainT_ = 0.f;
                    if (P_ > 92.0) missedT_ = 0.001f;                        // it missed it
                }
            }
            break;
        }
        case T_OUT: {
            // leaving without the cub, into the tunnel ahead; the next one comes after the gap
            const double u = std::min(1.0, trainT_ / (double)T_OUT_S);
            trainX_ = XB - DOOR + 150.0 * u * u;
            if (u >= 1.0) { train_ = T_AWAY; trainT_ = 0.f; }
            break;
        }
        case T_RIDE: {
            rideT_ += wdt;
            const double u = rideT_ / rideS();
            const double x = XB - DOOR + (XA - XB) * rideF(u);
            vel_ = (x - trainX_) / std::max((double)wdt, 1e-4);
            trainX_ = x;
            door_ = 0.f;
            P_ = trainX_ + DOOR;
            if (u >= 1.0) { train_ = T_TERM; leaveT_ = 0.f; P_ = XA; vel_ = 0.0; }
            break;
        }
        case T_TERM:
            trainX_ = XA - DOOR;
            if (P_ > XA + 1.15) leaveT_ += wdt;
            door_ = std::clamp(door_ + wdt / 1.0f * (leaveT_ > 0.5f ? -1.f : 1.f), 0.f, 1.f);
            if (leaveT_ > 0.5f && door_ <= 0.f) { train_ = T_LEAVE; trainT_ = 0.f; }
            break;
        case T_LEAVE: {
            const double u = trainT_ / 14.0;
            trainX_ = XA - DOOR - 170.0 * u * u;
            if (u >= 1.0) train_ = T_GONE;
            break;
        }
        default: trainX_ = -1000.0; break;
        }
        if (missedT_ > 0.f) missedT_ += wdt;
        if (missedT_ > 12.f || P_ >= XB - 0.03) missedT_ = 0.f;
        speed = (float)std::min(vel_, 3.0);
        travelling = !riding && (lineWalk || vel_ > 0.12);
        { // (testing: REFALL_LOSTPANDA_TRAINX=X holds the train with its rear at screen x X, both cabs in view)
            static const char* tx = std::getenv("REFALL_LOSTPANDA_TRAINX");
            if (tx) trainX_ = (C_ - xP_) + std::atof(tx);
        }
        // the camera follows, lagging a little (the cub moves ahead on the screen while it walks); it stops at the end.
        // On the train it keeps to the ride's own course (rideCamF: the train pulls out across the screen, the camera
        // catches up, then leads it into the terminus), moving with it and easing out any gap left from the platform
        const bool ridingNow = train_ == T_RIDE;
        const double c0 = C_;
        double cT = std::min(P_, C_END);
        if (ridingNow) {
            cT = XB + (XA - XB) * rideCamF(rideT_ / rideS());
            C_ += (XA - XB) * (rideCamF(rideT_ / rideS()) - rideCamF((rideT_ - wdt) / rideS()));
        }
        C_ += (cT - C_) * (1.0 - std::exp(-(ridingNow ? 2.5 : 1.1) * wdt));
        if (wdt > 0.f) camV_ = (C_ - c0) / wdt;                                // (the motion blur's speed)
        camY_ = approach(camY_, (float)(pathY(P_) - GY0), ridingNow ? 3.f : 1.6f, wdt);
        // the walk cycle: steps follow the ground covered (not while riding)
        const float stride = 0.32f * kidH_;
        if (!ridingNow) kid_.phase += (float)(vel_ * wdt) * TAU / stride;
        kid_.walk = approach(kid_.walk, ridingNow ? 0.f : saturate(speed / 1.1f), 5.f, wdt);
        reunion_ = smoothstepf((float)(C_END - 12.0), (float)pEnd_, (float)P_);
        strength_ = approach(strength_, (float)smd(20.0, C_END - 30.0, P_), 0.5f, wdt);
        munchT_ = noticeT_ > 0.f ? 0.f : std::max(0.f, munchT_ - wdt);   // (no more eating once it sees its family)
    }
    const Look look0 = lookAt(C_);
    Look look = look0;
    stageAt_ = placeOf(P_);
    stageName_ = STAGE[stageAt_].name;
    if (P_ >= C_END - 1.0 && arriveT_ > 0.f) stageName_ = "home";

    // ---- The pandas' life.
    updateKid(wdt, look.mood, stageAt_, travelling, speed);
    {
        // the family waits on the right; when the cub sees them they stand up and open their arms; then the hug
        const bool arrived = P_ >= pEnd_ - 0.05;
        arriveT_ = arrived ? arriveT_ + wdt : 0.f;
        for (int i = 0; i < 2; i++) {
            Pose& fp = i == 0 ? mom_ : dad_;
            Pose t = fp;
            const float tt = time_ * (i == 0 ? 0.35f : 0.27f) + i * 2.f;
            const bool seen = noticeT_ > 0.6f;
            t.mood = seen ? 1.f : 0.6f;
            t.sit = (seen && arriveT_ < 4.5f) ? 0.f : 1.f;
            t.turn = seen ? (arrived ? 0.7f : 0.3f) : 0.35f * std::sin(tt) + 0.25f;    // (their +x: toward the cub, on their left)
            t.lookX = t.turn > 0.f ? 1.f : -1.f;
            t.lookY = 0.f;
            t.arms = (seen && !arrived) ? 0.85f : 0.f;
            t.hug = (arrived && i == 0) ? 1.f : 0.f;
            t.tilt = arrived ? 0.14f : 0.05f * std::sin(tt * 1.3f);
            t.lean = arrived ? (i == 0 ? 0.8f : 0.4f) : 0.f;
            t.joyEyes = arrived ? smoothstepf(0.25f, 0.75f, 0.5f + 0.5f * std::sin(time_ * 0.31f + i * 2.1f + 1.f)) : (seen ? 1.f : 0.f);
            t.wind = 0.2f;
            updatePose(fp, wdt, t, 1.5f);
        }
    }
    // the butterfly: it flutters about the cub in the fields and the forest
    {
        const float want = overT_ < 0.f ? look.butterfly : 0.f;
        flyA_ = approach(flyA_, want, 0.5f, wdt);
        const vec2 head = V(xP_ + (float)(P_ - C_), GY + kidH_ * 0.9f);
        const vec2 goal = V(head.x + 1.6f * std::sin(time_ * 0.37f) + 0.9f * std::sin(time_ * 0.91f + 1.f) + 0.6f,
                            head.y + 0.6f + 0.9f * std::sin(time_ * 0.53f + 2.f) + 0.4f * std::sin(time_ * 1.7f));
        if (flyA_ < 0.02f) fly_ = V(head.x + 6.f, head.y + 3.f);
        flyV_ = V(flyV_.x + (goal.x - fly_.x) * 2.2f * wdt, flyV_.y + (goal.y - fly_.y) * 2.2f * wdt);
        flyV_ = V(flyV_.x * std::exp(-1.6f * wdt), flyV_.y * std::exp(-1.6f * wdt));
        fly_ = V(fly_.x + flyV_.x * wdt, fly_.y + flyV_.y * wdt);
        fly_.x = std::min(fly_.x, layout::LEFT_COLUMN[0] - 0.8f);
        flyFlap_ += wdt * 14.f;
    }

    // ---- Lights and motes.
    for (Light& l : lights_) l.age += wdt * (1.f - 0.4f * slow_) + dt * 0.4f * slow_;
    lights_.erase(std::remove_if(lights_.begin(), lights_.end(), [](const Light& l) { return l.age >= l.life; }), lights_.end());
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

    // background sprites: the lights of hope, the snacks on their way, the fireflies
    bgSprites_.clear();
    fgSprites_.clear();
    // the setup: the camera comes down out of the sky to the cub; the teardown: it rises back into the sky
    viewY_ = camY_ + 9.f * (1.f - smoothstepf(0.f, 0.85f, f.setup) + smoothstepf(0.05f, 1.f, f.teardown));
    const vec2 kidHead = V(xP_ + (float)(P_ - C_), GY + (float)(pathY(P_) - GY0) - viewY_ + kidH_ * 0.55f);
    for (Light& l : lights_) {
        if (l.age < 0.f) continue;
        if (l.kind > 0) {
            // red sparkles: a little comet of them along an arc from the row to the cub, twinkling and turning,
            // melting into it at the end
            const float u = saturate(l.age / l.life);
            const vec2 b = kidHead;
            auto at = [&](float uu) {
                const float e = uu * uu * (3.f - 2.f * uu), iu = 1.f - e;
                return V(iu * iu * l.a.x + 2.f * iu * e * l.ctl.x + e * e * b.x, iu * iu * l.a.y + 2.f * iu * e * l.ctl.y + e * e * b.y);
            };
            const float a = smoothstepf(0.f, 0.08f, u) * (1.f - smoothstepf(0.9f, 1.f, u)) * world;
            const int n = l.kind == 2 ? 9 : 6;
            for (int k = 0; k < n; k++) {
                const float uk = u - k * 0.03f;
                if (uk <= 0.f) break;
                vec2 pk = at(uk);
                const float wob = l.seed * 37.f + k * 2.3f;
                pk.x += 0.18f * std::sin(l.age * 5.f + wob) * (k > 0);
                pk.y += 0.18f * std::cos(l.age * 4.3f + wob) * (k > 0);
                const float tw = 0.7f + 0.3f * std::sin(l.age * 14.f + wob);
                const float sz = l.size * (1.9f - 0.13f * k) * (1.f - 0.4f * smoothstepf(0.85f, 1.f, u));
                const vec3 red = k % 3 == 1 ? vec3(1.6f, 0.45f, 0.35f) : vec3(1.6f, 0.12f, 0.16f);
                sprite(u < 0.15f ? bgSprites_ : fgSprites_, pk, sz, red, a * tw * (1.f - 0.07f * k), 7, l.age * 1.6f + wob);
            }
            if (u > 0.92f && !l.fed) {
                l.fed = true;
                joy_ = std::max(joy_, l.kind == 2 ? 0.7f : 0.3f);
                if (l.kind == 2 && act_ != A_HUG) { act_ = A_HOP; actT_ = 0.f; actDur_ = 2.4f; }
            }
            continue;
        }
        const float u = saturate(l.age / (l.life * 0.72f));
        const float e = u * u * (3.f - 2.f * u);
        const float iu = 1.f - e;
        vec2 p = V(iu * iu * l.a.x + 2.f * iu * e * l.ctl.x + e * e * l.b.x, iu * iu * l.a.y + 2.f * iu * e * l.ctl.y + e * e * l.b.y);
        p.x += 0.25f * std::sin(l.age * 2.1f + l.seed * 9.f);
        p.y += 0.25f * std::sin(l.age * 1.7f + l.seed * 5.f);
        const float t = l.age / l.life;
        const float a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.75f, 1.f, t));
        const float tw = 0.85f + 0.15f * std::sin(l.age * 6.f + l.seed * 20.f);
        sprite(bgSprites_, p, l.size * (1.f + 0.3f * (1.f - t)), l.col * 1.4f, a * tw * world, 4);
    }
    {
        const float n = (look.fireflies + 0.5f * dropAmt_) * world;
        const int count = (int)std::ceil(48.f * std::min(1.f, n));
        for (int i = 0; i < count; i++) {
            const float s = i * 1.618f;
            float x = viewC_.x - viewH_.x + std::fmod(s * 13.7f + 3.f, 2.f * viewH_.x);
            x += 1.5f * std::sin(time_ * (0.11f + 0.05f * std::sin(s)) + s * 4.f) + std::fmod(-(float)C_ * 0.6f, 2.f * viewH_.x) * 0.f;
            const float y = GY - 1.f + std::fmod(s * 7.3f, 12.f) + 1.2f * std::sin(time_ * (0.17f + 0.04f * std::cos(s)) + s);
            float blink = 0.5f + 0.5f * std::sin(time_ * (0.8f + 0.3f * std::sin(s * 3.f)) + s * 7.f);
            blink = blink * blink;
            if ((i & 3) == 0) blink = std::max(blink, nod * 0.9f);
            const float a = blink * smoothstepf((float)i, (float)i + 1.f, 48.f * std::min(1.f, n)) * (0.4f + 0.6f * kickS_ * ((i % 5) == 0));
            sprite(bgSprites_, V(x, y), 0.32f, lerp(lin(0xd8ff90), lin(vd.light), 0.3f), a * 0.9f + 0.1f * a, 4);
        }
    }
    sprites_.clear();
    if (!f.bare) {
        for (const Mote& m : motes_) {
            const float t = m.age / m.life;
            const float a = smoothstepf(0.f, 0.1f, t) * (1.f - smoothstepf(0.5f, 1.f, t)) * m.alpha;
            sprite(sprites_, m.p, m.size * (1.f + t), m.col, a, 0);
        }
    }
    cells_.clear();
    if (!f.bare) buildBoard(game);

    const float dawn = (float)smd(1201.0, 1326.0, P_);
    auto setTrain = [&](GLuint prog, float px) {
        set1f(prog, "uTrainX", (float)trainX_);
        set1f(prog, "uDoor", smoothstepf(0.f, 1.f, door_));
        set1f(prog, "uTrainLit", 1.f);
        set1f(prog, "uTrainBody", 0.55f + 0.7f * dawn);
        // the straps swing harder the faster it runs; the motion blur over a 1/30 s shutter (the world past the
        // camera, the tunnel's lamps past the train)
        set1f(prog, "uSway", train_ == T_RIDE ? 1.f + 1.5f * smoothstepf(0.f, 50.f, (float)vel_) : 0.25f);
        set1f(prog, "uBlur", (float)std::min(3.0, std::fabs(camV_) / 30.0));
        set1f(prog, "uRunBlur", train_ == T_RIDE ? (float)std::min(3.0, std::fabs(vel_) / 30.0) : 0.f);
        set1f(prog, "uTrainT", trainClock_);
        set1f(prog, "uDir", train_ == T_LEAVE ? -1.f : 1.f);
        set1f(prog, "uHead", (1.f - 0.8f * dawn) * (train_ == T_AWAY || train_ == T_GONE ? 0.f : 1.f));
        (void)px;
    };
    const bool trainOn = std::fabs(trainX_ + 17.0 - (C_ - xP_)) < viewH_.x + 40.0;
    auto drawFront = [&]() {
        if (!trainOn) return;
        glUseProgram(progFront_);
        set2f(progFront_, "uViewC", viewC_.x, viewC_.y);
        set2f(progFront_, "uViewH", viewH_.x, viewH_.y);
        set1f(progFront_, "uD", (float)C_ - xP_);
        set1f(progFront_, "uCamY", viewY_);
        set1f(progFront_, "uPxP", 1.f / pxPerUnit_);
        set1f(progFront_, "uWorldA", world);
        set1f(progFront_, "uOver", smoothstepf(0.f, 1.f, over_));
        set3f(progFront_, "uBridgeCol", bridge_.color);
        setTrain(progFront_, 1.f / pxPerUnit_);
        ugl::drawFullscreen();
    };
    // the world shader's uniforms (the world pass, and the near bamboo's pass over the pandas)
    auto setWorld = [&](GLuint pw, float pxp) {
        glUseProgram(pw);
        set2f(pw, "uViewC", viewC_.x, viewC_.y);
        set2f(pw, "uViewH", viewH_.x, viewH_.y);
        set1f(pw, "uPxP", pxp);
        set1f(pw, "uD", (float)C_ - xP_);
        set1f(pw, "uCamY", viewY_);
        set1f(pw, "uDawn", dawn);
        set1f(pw, "uSubLight", 1.f);
        setTrain(pw, pxp);
        set1f(pw, "uTime", time_);
        set1f(pw, "uBeat", (float)std::fmod(beat, 4096.0));
        set1f(pw, "uPeak", wp);
        set1f(pw, "uMid", wm);
        set1f(pw, "uDrop", std::min(1.f, dropAmt_ + swell_ * 0.3f));
        // the world's saturation: muted while the cub is lost (its sadness), full and a little more in a drop, full
        // again as it nears its family
        set1f(pw, "uWorldSat", std::max(0.58f + 0.42f * reunion_, 0.58f + 0.62f * std::min(1.f, dropAmt_ + swell_ * 0.3f)));
        set1f(pw, "uKick", kickS_);
        set1f(pw, "uRain", look.rain);
        set1f(pw, "uStars", look.stars);
        set1f(pw, "uClouds", look.clouds);
        set1f(pw, "uOver", smoothstepf(0.f, 1.f, over_));
        set1f(pw, "uWorldA", world);
        set1f(pw, "uSlow", slow_);
        set3f(pw, "uSkyTop", look.skyTop);
        set3f(pw, "uSkyHor", look.skyHor);
        set3f(pw, "uFog", look.fog);
        set3f(pw, "uSunCol", look.sunCol);
        set4f(pw, "uSun", look.sunX, look.sunY - viewY_ * 0.08f, look.sunR, look.sun);
        set3f(pw, "uBridgeCol", bridge_.color);
        set1f(pw, "uLamp", look.lamp * (1.f + 0.15f * wp + 0.2f * dropAmt_));
    };
    // the cub is inside the train (behind its near side) from its door to the terminus
    const bool inside = (train_ == T_WAIT && P_ > XB - 0.35) || train_ == T_RIDE || (train_ == T_TERM && P_ < XA + 0.35);

    // =========================================================== the world (full resolution, up to 1440 lines)
    {
        worldT_.bind();
        glDisable(GL_BLEND);
        // the city's layers show until its street (at half speed) leaves the screen's left edge past 1290 along the
        // way; the countryside's from when the works' warehouses (at 0.4) reach its right edge at 1245
        const double ud = C_ - xP_;
        const bool city = ud + 2.0 * (viewC_.x - viewH_.x) < 1290.0, country = ud + 2.5 * (viewC_.x + viewH_.x) > 1245.0;
        const GLuint pw = !country ? progWorldCity_ : (!city ? progWorldCountry_ : progWorld_);
        setWorld(pw, 2.f * viewH_.y / worldT_.h);
        ugl::drawFullscreen();
    }

    // =========================================================== the scene (full resolution)
    sceneT_.bind();
    glUseProgram(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progComp_, "uWorld", 0);
    // (sharpened only when upscaled: a texel of 0 leaves it as it is)
    const bool up = worldT_.h < height_;
    set2f(progComp_, "uTexel", up ? 1.f / worldT_.w : 0.f, up ? 1.f / worldT_.h : 0.f);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawSprites(bgSprites_, true);
    // the train's near side (when the cub is not in it), the family, the cub, the train's near side over it (when it
    // rides), the snacks reaching it, the butterfly
    if (!inside) drawFront();
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    {
        const float alpha = world;
        const float fs = famS_;
        const float shift = (float)(C_END - C_);
        const float fy = -viewY_;
        if (shift < 2.f * viewH_.x + 12.f) {
            drawPanda(dad_, V(dadEndX_ + shift, GY + 0.7f * fs + fy), 6.9f * fs, -1.f, 1.f, alpha, 0.06f, false, 0);
            drawPanda(mom_, V(momEndX_ + shift, GY + 0.35f * fs + fy), 6.6f * fs, -1.f, 1.f, alpha, 0.03f, false, 0);
        }
        const float ky = GY + (float)(pathY(P_) - GY0) - viewY_;
        {
            const double x0 = trainX_ + 17.6;
            kidRoll_ = inside ? (float)std::atan((trackY(x0 + 17.0) - trackY(x0)) / 17.0) : 0.f;
        }
        kid_.tilt += 0.035f * nod;
        drawPanda(kid_, V(xP_ + (float)(P_ - C_), ky), kidH_, 1.f, 0.f, alpha, 0.f, P_ < 80.0 || (P_ > 98.0 && P_ < XB - 0.7), 0);
        kid_.tilt -= 0.035f * nod;
    }
    if (inside) drawFront();
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // the near bamboo, in front of the way and of the pandas
    if (C_ - xP_ + viewC_.x > 1380.0) {
        setWorld(progNear_, 1.f / pxPerUnit_);
        ugl::drawFullscreen();
    }
    // hearts rising from the pandas when they meet (on the right of the HUD, behind nothing of the board)
    {
        const float fs = famS_;
        const float shift = (float)(C_END - C_);
        if (arriveT_ > 0.15f && overT_ < 0.f) {
            heartT_ -= wdt;
            if (heartT_ <= 0.f && hearts_.size() < 40) {
                const int who = rng_.irange(0, 2);
                vec2 at = who == 0 ? V(xP_ + (float)(P_ - C_), GY + kidH_ * 0.8f)
                        : who == 1 ? V(momEndX_ + shift, GY + 0.35f * fs + 6.6f * fs * 0.8f)
                                   : V(dadEndX_ + shift, GY + 0.7f * fs + 6.9f * fs * 0.8f);
                Heart h;
                h.p = V(at.x + rng_.range(-1.2f, 1.2f), at.y + rng_.range(-0.5f, 0.8f));
                h.life = rng_.range(3.5f, 5.f);
                h.size = rng_.range(0.34f, 0.6f) * fs;
                h.seed = rng_.uniform();
                h.col = lerp(lin(0xff7a90), lin(0xe8344e), rng_.uniform());
                hearts_.push_back(h);
                heartT_ = arriveT_ < 4.f ? rng_.range(0.2f, 0.35f) : rng_.range(0.5f, 0.9f);
            }
        }
        const float minX = layout::RIGHT_COLUMN[2] + 0.5f;
        for (Heart& h : hearts_) {
            h.age += wdt;
            h.p.y += wdt * (0.6f + 0.25f * h.seed);
        }
        hearts_.erase(std::remove_if(hearts_.begin(), hearts_.end(), [](const Heart& h) { return h.age >= h.life; }), hearts_.end());
        for (const Heart& h : hearts_) {
            const float t = h.age / h.life;
            const float a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.55f, 1.f, t)) * world;
            const float x = std::max(minX + h.size, h.p.x + 0.35f * std::sin(h.age * 1.4f + h.seed * 6.f));
            sprite(fgSprites_, V(x, h.p.y), h.size * (0.8f + 0.2f * smoothstepf(0.f, 0.2f, t)), h.col, a, 6, 0.15f * std::sin(h.age * 1.5f + h.seed * 9.f));
        }
    }
    drawSprites(fgSprites_, false);
    if (flyA_ > 0.01f) {
        std::vector<SpriteInst> fly;
        sprite(fly, fly_, 0.32f, lerp(lin(0xfff3c0), lin(vd.accent), 0.35f), flyA_ * world, 3, 0.5f + 0.5f * std::sin(flyFlap_));
        drawSprites(fly, false);
    }
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", lerp(look.veil, bridge_.color, bridge));
        set3f(progVeil_, "uRail", lerp(lin(vd.light) * 0.35f, bridge_.ink * 0.4f, bridge));
        set1f(progVeil_, "uWellA", 0.9f + 0.07f * smoothstepf(0.1f, 0.6f, ugl::luma(look.skyHor)));
        set1f(progVeil_, "uColA", 0.5f * world);
        set1f(progVeil_, "uRailA", 0.5f);
        set1f(progVeil_, "uPx", pxPerUnit_);
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], -5.f, layout::LEFT_COLUMN[3]);
        set4f(progVeil_, "uColR", 5.f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();
    }
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set3f(progCell_, "uLightC", lerp(look.key, vec3(1.f), 0.75f));
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        // the eye the cubes are seen from: over the well's middle, far off, looking down a little
        set1f(progCell_, "uEyeX", 0.f);
        set1f(progCell_, "uEyeD", 110.f);
        set1f(progCell_, "uTilt", 0.06f);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        // their contact shadows first (on what is behind), then the cubes
        set1f(progCell_, "uPass", 1.f);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
        set1f(progCell_, "uPass", 0.f);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    drawSprites(sprites_, false);
    drawHud(f.panels);
    fade_ = f.fade;
    // Test hook: REFALL_LOSTPANDA_SHEET=1 draws, over everything, the pandas' model sheet big on a plain ground under a
    // plain white light (the cub facing us happy and surprised, in profile; =2 its other poses, the parents), to set
    // their drawing against the owner's reference (with REFALL_BARE=1 and --review-shot).
    static const int sheet = std::getenv("REFALL_LOSTPANDA_SHEET") ? std::atoi(std::getenv("REFALL_LOSTPANDA_SHEET")) : 0;
    if (sheet > 0) drawSheet(sheet);
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

const UniverseEntry& universe_lostpanda() {
    static const UniverseEntry e{
        "LOST PANDA", "a little panda lost in the city at night; every cleared line takes it further on its way home, through the subway, the canal at dawn and the bamboo forest to its family",
        [] { return std::unique_ptr<Universe>(new LostPandaUniverse()); },
        // Fits most songs; the gentle, warm ones a little better.
        [](const SongLook& l) { return 0.85f + 0.15f * (1.f - l.density) + 0.1f * (1.f - l.bassWeight); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
