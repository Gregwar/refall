// DOCKS: a container port at dusk or by night, in 3D. The blocks are shipping containers.
//
// One world: we stand on a quay, under a gantry crane whose legs frame the screen; the well is a bay painted on the
// quay where containers are stacked. Across the basin, two container ships lie at the far quay, bow to bow, under the
// tall ship-to-shore cranes; behind them the terminal's yard of stacked boxes, its lamp masts, sheds and offices, and
// beyond, the far shore's hills and the town's lights under the sky of the hour (dusk, the blue hour, night, dawn, a
// north sea night, an afterglow). The basin mirrors it all (a reflection pass, rippled), the haze softens the distance.
//
// The pieces are shipping containers, one per cell: steel boxes with corner posts and castings, corrugated walls and
// their doors facing us (two leaves, the locking rods), each of the seven piece types a clean color of the variant.
// The falling piece hangs from the gantry: a slim lifting beam rests on it, hairline cables run up out of frame; it
// follows the piece exactly (moves, rotations, drops) and lets go when the piece lands, rising away. The ghost is a
// thin outline. A cleared row is taken by other cranes: its containers are lifted swiftly back out of the well, then
// carried across the basin on their cables and set down on the ships' decks, the left half on the left ship and the
// right half on the right one: the ships fill up over the song, bay after bay from their deck houses. A full ship
// casts off and leaves, an empty one comes in to the berth.
//
// The song: calm is a quiet port at rest (few lamps, cranes idle, thicker haze); mid wakes it up (more lamps, the
// cranes work on the beat grid, a move every two beats); a peak lights every mast, the cranes work faster, their red
// beacons blink with the grid and the kicks spark their trolleys' lamps. The drop switches the port to full light:
// the masts come on in a sequence from the well outward, the colors deepen and the sky glows (one swell that settles
// over seconds). A Tetris flashes the cranes' and masts' lamps in a wave and the ships' decks light up. Slow motion is
// a sea fog rolling in (the lamps haloed, the port cooled); game over lifts the stack away, container by container,
// from the top.
//
// Layout contract: the reference camera raised to EYE_Y and looking straight ahead, its image shifted back down (a
// lens shift): the board's plane keeps its pixels, the port recedes in perspective.
#include "universes/docks/docks.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/docks/shaders.hpp"
#include "universes/docks/world.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

using namespace docks_world;

namespace {

// Barlow, SIL Open Font License, see OFL-Barlow.txt.
const unsigned char FONT_BARLOW[] = {
#include "universes/docks/Barlow-Medium.ttf.inc"
};

namespace shaders = docks_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }

// An hour at the port (sRGB): the sky (zenith, middle, horizon, the sun's glow), the low sun's light, the haze, the
// ambient light (sky, ground), the work lights over the well, the water, the lamps (sodium, LED), the concrete, the
// cranes' paint (main, second), the seven containers (I O T S Z J L) and three hulls; the sun's azimuth and height,
// stars, haze density, the moon, the low haze over the water, how bright the lamps are.
struct VariantDef {
    const char* name;
    unsigned zen, mid, hor, glow, sun, fog, amb, ground, key, water, lampA, lampB, quay, crane, crane2;
    unsigned piece[7];
    unsigned hull[3];
    float sunAz, sunEl, stars, fogD, moon, haze, lampK, keyK, ambK;
};
const VariantDef VARIANTS[] = {
    // the sun just set behind the terminal: an orange horizon, sodium lamps coming on, white and red cranes
    {"SODIUM DUSK", 0x1a2546, 0x4a4a74, 0xe8865a, 0xff9248, 0x8a5a40, 0x6a5a70, 0x2e3450, 0x1c1a22, 0xfff0dc, 0x1a2a3e, 0xffa24a,
     0xfff0d8, 0x8a8680, 0xd8dce2, 0xc8382c,
     {0x2e9ee0, 0xf2c23a, 0x8e62d6, 0x3fb46a, 0xe2463a, 0x2c58c8, 0xf2862a}, {0x1a2a44, 0x6a1c1c, 0x1e3a30},
     -0.32f, 0.02f, 0.12f, 0.0042f, 0.f, 0.6f, 1.f, 1.f, 1.f},
    // the deep blue after sunset: a cobalt sky, amber lamps, blue cranes
    {"BLUE HOUR", 0x0a1434, 0x1c3468, 0x5876ae, 0xb08098, 0x34406a, 0x2a3a5c, 0x22304e, 0x101420, 0xeef2ff, 0x0e1a30, 0xffa040,
     0xd8e6ff, 0x767a84, 0x2c6cc4, 0xe6e8ea,
     {0x3fb2e8, 0xf0d060, 0xd04a8e, 0x2cb59c, 0xf0664a, 0x4a6ae0, 0xe89a3a}, {0x22283a, 0x7a2420, 0x1c3a4a},
     0.25f, 0.f, 0.35f, 0.0040f, 0.f, 0.5f, 1.f, 1.f, 1.f},
    // the night shift: stars and a low moon, white LED floods, yellow cranes
    {"NIGHT SHIFT", 0x03060e, 0x0a1426, 0x1c2a42, 0x34405a, 0x182238, 0x141c2e, 0x121a2a, 0x080a10, 0xf0f6ff, 0x081220, 0xffb060,
     0xdceaff, 0x6a6c70, 0xf0b428, 0x34383e,
     {0x30c8e8, 0xf2e04c, 0xb08af0, 0x82d860, 0xf05a78, 0x4a7cf0, 0xff9a3c}, {0x1c2232, 0x5e1a1a, 0x16302a},
     0.45f, 0.06f, 1.f, 0.0036f, 1.f, 0.45f, 1.1f, 1.f, 0.9f},
    // first light: a lilac sky warming to peach, the morning mist on the basin, the lamps going pale; red cranes
    {"DAWN", 0x3a4a7a, 0x8a8cb4, 0xf0bc9c, 0xffcc98, 0xc89070, 0x9a94aa, 0x5e6888, 0x34343c, 0xfff6ec, 0x3e5070, 0xffb060,
     0xfff0e0, 0xa4a09a, 0x5a6c8e, 0xe8eaee,
     {0x1670c4, 0xe0a010, 0x6438b4, 0x1a8a4c, 0xc42624, 0x1c3a98, 0xd85a10}, {0x1a2a44, 0x6a1c1c, 0x203a32},
     -0.2f, 0.035f, 0.f, 0.0026f, 0.f, 0.4f, 0.55f, 0.95f, 1.6f},
    // a north sea night: low cloud, a grey-teal sky, cold lamps, green cranes
    {"NORTH SEA", 0x0a1418, 0x1c2e34, 0x3e5658, 0x48605e, 0x2a3c3c, 0x2a3c40, 0x1c2c30, 0x0c1012, 0xe6f2f4, 0x0a181c, 0xffae5a,
     0xcaf0ff, 0x6e7270, 0x3a8a7a, 0xe0e0d8,
     {0x3cb8c8, 0xe8d26a, 0xa48ad8, 0x6cc48a, 0xe87060, 0x5288d8, 0xe8a254}, {0x1a2428, 0x5a1e1a, 0x223028},
     0.1f, 0.f, 0.f, 0.0050f, 0.f, 0.9f, 1.f, 1.f, 1.f},
    // the afterglow: a violet sky, a magenta horizon, white cranes
    {"AFTERGLOW", 0x140a2a, 0x3a2058, 0xd04e78, 0xff6a58, 0x6a2a40, 0x4a2a4c, 0x2a1e3c, 0x120c16, 0xfff0f0, 0x160e26, 0xffa050,
     0xf0e0ff, 0x7a7480, 0xe6e6ee, 0x2a2a3a,
     {0x38c0e0, 0xf6d050, 0x9a70f0, 0x50d090, 0xff5a6a, 0x4a70f0, 0xff9a40}, {0x1c1c30, 0x601c2a, 0x1c3038},
     -0.4f, 0.015f, 0.4f, 0.0042f, 0.f, 0.55f, 1.f, 1.f, 1.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float LIFT_T = 0.42f;     // a cleared row is lifted out of the well in this long (real time)
constexpr float CELL_H = 0.475f;    // a container's half size in the well
constexpr float CELL_D = 0.42f;     // its half depth

mat4 inverse(const mat4& m) {
    const float* a = m.m;
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
    const float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    mat4 r{};
    if (std::fabs(det) < 1e-12f) return mat4::identity();
    for (int i = 0; i < 16; i++) r.m[i] = inv[i] / det;
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
int glyphIndex(unsigned char ch) {
    if (ch < 32 || ch >= 128) ch = '?';
    return ch - 32;
}

void lerpPal(vec3& a, const vec3& b, float k) { a = lerp(a, b, k); }

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

void DocksUniverse::setupBoxVao(GLuint& vao, GLuint& vbo) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < 5; i++) {
        glEnableVertexAttribArray(2 + i);
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Box), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(2 + i, 1);
    }
    glBindVertexArray(0);
}

bool DocksUniverse::init(int width, int height) {
    using shaders::withCommon;
    progSky_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::SKY_FS).c_str(), "docks sky");
    progWater_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::WATER_FS).c_str(), "docks water");
    progBox_ = ugl::program(shaders::BOX_VS, withCommon(shaders::BOX_FS, true).c_str(), "docks solids");
    progSprite_ = ugl::program(withCommon(shaders::SPRITE_VS).c_str(), withCommon(shaders::SPRITE_FS).c_str(), "docks lamps");
    progLine_ = ugl::program(shaders::LINE_VS, withCommon(shaders::LINE_FS).c_str(), "docks cables");
    progCone_ = ugl::program(withCommon(shaders::CONE_VS).c_str(), withCommon(shaders::CONE_FS).c_str(), "docks light cones");
    progVeil_ = ugl::program(shaders::VEIL_VS, shaders::VEIL_FS, "docks veil");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "docks down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "docks blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "docks final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "docks shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "docks text");

    // a unit cube (-1..1): 36 vertices, position + normal
    {
        std::vector<float> v;
        const int faces[6][3] = {{0, 1, 2}, {0, -1, 2}, {1, 1, 2}, {1, -1, 2}, {2, 1, 0}, {2, -1, 0}};
        for (int f = 0; f < 6; f++) {
            const int ax = faces[f][0];
            const float sg = (float)faces[f][1];
            const int a1 = (ax + 1) % 3, a2 = (ax + 2) % 3;
            const float corners[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
            for (int k = 0; k < 6; k++) {
                float p[3], n[3] = {0, 0, 0};
                p[ax] = sg;
                // keep the winding counter-clockwise seen from outside
                const float u = corners[k][0], w = corners[k][1] * sg;
                p[a1] = u;
                p[a2] = w;
                n[ax] = sg;
                v.insert(v.end(), {p[0], p[1], p[2], n[0], n[1], n[2]});
            }
        }
        glGenBuffers(1, &cubeVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    }
    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    setupBoxVao(staticVao_, staticVbo_);
    setupBoxVao(boxVao_, boxVbo_);
    // lamps: the quad + (position radius, color intensity) per instance
    glGenVertexArrays(1, &spriteVao_);
    glGenBuffers(1, &spriteVbo_);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    for (int i = 0; i < 2; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Sprite), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    // cables: a strip (along 0..1, across -1..1) + three vec4 per instance
    {
        const float strip[8] = {0, -1, 1, -1, 0, 1, 1, 1};
        GLuint stripVbo = 0;
        glGenVertexArrays(1, &lineVao_);
        glGenBuffers(1, &lineVbo_);
        glGenBuffers(1, &stripVbo);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, stripVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(strip), strip, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        for (int i = 0; i < 3; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Line), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
        // the strip's buffer stays bound to the VAO (freed with the context)
    }
    glGenVertexArrays(1, &quadVao_);
    glBindVertexArray(quadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
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

    buildFont(font_, FONT_BARLOW);
    resize(width, height);
    // the cranes: three working each berth
    cranes_.clear();
    const float cx[6] = {-40.f, -26.5f, -13.f, 11.5f, 24.f, 37.5f};
    for (int i = 0; i < 6; i++) {
        Crane c;
        c.x = c.x0 = cx[i];
        c.ship = i < 3 ? 0 : 1;
        c.trolley = c.tFrom = c.tTo = 0.25f + 0.35f * hashf(i * 7 + 1);
        c.hoist = c.hFrom = c.hTo = 0.2f;
        c.colorIdx = i;
        cranes_.push_back(c);
    }
    // two idle cranes beyond the ships' sterns, their booms raised
    for (int s = -1; s <= 1; s += 2) {
        Crane c;
        c.x = c.x0 = s * 58.f;
        c.ship = -1;
        c.raise = 1.f;
        cranes_.push_back(c);
    }
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    newShip(0, true);
    newShip(1, true);
    return glGetError() == GL_NO_ERROR;
}

void DocksUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] docks font init failed\n");
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

// The layout's reference camera raised to EYE_Y and looking straight ahead, its image shifted back down: the board
// plane lands exactly on the reference camera's pixels.
void DocksUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera ref = layout::camera(width_, height_);
    const float aspect = (float)width_ / height_;
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    const float focal = 1.f / std::tan(half);
    eye_ = vec3(ref.target.x, EYE_Y, ref.target.z + ref.dist);
    const mat4 view = mat4::lookAt(eye_, eye_ + vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f));
    mat4 shift = mat4::identity();
    shift.at(1, 3) = focal * (EYE_Y - ref.target.y) / ref.dist;
    vp_ = shift * mat4::perspective(2.f * half, aspect, 0.5f, 900.f) * view;
    invVp_ = inverse(vp_);
    mirror_ = mat4::identity();
    mirror_.at(1, 1) = -1.f;
    mirror_.at(1, 3) = 2.f * Y_W;

    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false);
    reflT_.create(std::max(1, width_ / 2), std::max(1, height_ / 2), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    if (msFbo_) {
        glDeleteFramebuffers(1, &msFbo_);
        glDeleteRenderbuffers(1, &msColor_);
        glDeleteRenderbuffers(1, &msDepth_);
    }
    glGenFramebuffers(1, &msFbo_);
    glGenRenderbuffers(1, &msColor_);
    glGenRenderbuffers(1, &msDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA16F, width_, height_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_DEPTH_COMPONENT24, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

DocksUniverse::~DocksUniverse() {
    for (ugl::Target* t : {&sceneT_, &reflT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progSky_, progWater_, progBox_, progSprite_, progLine_, progCone_, progVeil_, progDown_, progBlur_, progFinal_, progShape_, progText_})
        glDeleteProgram(p);
    for (GLuint v : {staticVao_, boxVao_, spriteVao_, lineVao_, quadVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {cubeVbo_, quadVbo_, staticVbo_, boxVbo_, spriteVbo_, lineVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (font_.tex) glDeleteTextures(1, &font_.tex);
    delete (stbtt_fontinfo*)font_.info;
}

// =====================================================================================================
// Song, level
// =====================================================================================================

vec3 DocksUniverse::pieceColor(int type) const { return pal_.piece[std::clamp(type, 0, 6)]; }

vec3 DocksUniverse::cargoColor(int idx) const {
    if (idx >= 0 && idx < 7) return palT_.piece[idx];
    if (idx == 7) return lin(0xd6d6d0);
    if (idx == 8) return lin(0x50555c);
    return lin(0x8c4c34);
}

void DocksUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.4f) v = 2;                                  // night shift
        else if (t.hasSong && t.bassWeight > 0.62f && t.dynamics > 0.5f) v = 5;                   // afterglow
        else if (t.hasSong && t.minor && t.airWeight > 0.55f) v = 4;                              // north sea
        else if (t.hasSong && !t.minor && t.brightness > 0.62f) v = 3;                            // dawn
        else if (t.hasSong && t.minor) v = 1;                                                     // blue hour
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    const bool changed = v != variant_ || !colSet_;
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    Palette& P = palT_;
    P.zen = lin(V.zen); P.mid = lin(V.mid); P.hor = lin(V.hor); P.glow = lin(V.glow); P.sun = lin(V.sun) * 0.25f;
    P.fog = lin(V.fog); P.amb = lin(V.amb) * 0.55f * V.ambK; P.ground = lin(V.ground) * 0.5f * V.ambK;
    P.key = lin(V.key) * 0.95f * V.keyK; P.water = lin(V.water);
    P.lampA = lin(V.lampA); P.lampB = lin(V.lampB); P.quay = lin(V.quay) * 0.55f; P.crane = lin(V.crane); P.crane2 = lin(V.crane2);
    for (int i = 0; i < 7; i++) P.piece[i] = lin(V.piece[i]);
    for (int i = 0; i < 3; i++) P.hull[i] = lin(V.hull[i]);
    P.sunAz = V.sunAz; P.sunEl = V.sunEl; P.stars = V.stars; P.fogD = V.fogD * 0.8f; P.moon = V.moon; P.haze = V.haze; P.lampK = V.lampK;
    const bool instant = !colSet_ || seconds <= 0.1f;
    if (instant) { pal_ = palT_; colSet_ = true; }
    seed_ = t.seed ? t.seed : 1;
    seedF_ = (float)((seed_ >> 20) % 1000) * 0.137f;
    rng_ = Rng(seed_ ^ 0xD0C5ull);
    if (changed || instant || static_.empty()) buildPort();
}

void DocksUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    (void)impact;
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
}

// The drop: the port switches to full light, the masts in a sequence from the well outward.
void DocksUniverse::drop(float strength) {
    dropMode_ = true;
    dropT_ = 0.f;
    swellPeak_ = std::max(swellPeak_, 0.7f + 0.3f * strength);
}

void DocksUniverse::refresh(float strength) { refreshK_ = std::max(refreshK_, strength); }

vec2 DocksUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints DocksUniverse::hints() const {
    TransitionHints h;
    h.background = lerp(pal_.mid, pal_.fog, 0.5f) * 0.6f;
    h.light = false;
    h.ink = lerp(pal_.lampA, vec3(1.f), 0.55f) * 0.75f;
    h.accent = pal_.lampA;
    h.styles = TS_REBUILD | TS_LUMA | TS_CURTAIN;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void DocksUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) seeds_[c.y][c.x] = rng_.uniform();
        // the lifting beam lets go and rises away
        if (spr_.on) { spr_.rel = 0.f; spr_.relY = spr_.y; }
        break;
    }
    case GameEvent::Clear: {
        // each container of the row is taken by a crane: lifted back out of the well, then carried to a ship
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        std::sort(rows.begin(), rows.end());
        if (ev.count >= 4) tetrisT_ = 0.f;
        for (auto& c : ev.cells) {
            if (c.x < 0 || c.x >= Game::W || c.y < Game::HIDDEN) continue;
            const int k = (int)(std::find(rows.begin(), rows.end(), c.y) - rows.begin());
            Flight fl;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            fl.p0 = vec3(p.x, p.y, -CELL_D);
            fl.cur = fl.p0;
            fl.col = pieceColor(c.type);
            fl.seed = rng_.uniform();
            // out of the well: up and back, the row spreading a little toward its ship
            const float side = c.x < Game::W / 2 ? -1.f : 1.f;
            fl.p1 = vec3(p.x * 1.15f + side * 0.6f, p.y + 2.2f + 0.25f * k, -9.f - 0.4f * (float)(c.x % 3));
            int ship = side < 0 ? 0 : 1;
            int slot = reserveSlot(ship);
            if (slot < 0) { ship = 1 - ship; slot = reserveSlot(ship); }
            if (slot >= 0) {
                fl.ship = ship;
                fl.slot = slot;
                fl.p2 = slotPos(ship, slot);
            } else {
                // no ship at a berth: the container goes to the yard behind
                fl.ship = -1;
                fl.p2 = vec3(side * rng_.range(20.f, 70.f), Y_F + 4.5f, Z_YARD - 6.f);
            }
            fl.dur = rng_.range(2.8f, 3.4f) + 0.2f * k;
            fl.delay = 0.02f * (float)((c.x * 7) % 5);
            fl.kind = 0;
            fl.sizeK = 0.f;
            // the piece type in the seed's integer part (the cargo keeps its color)
            fl.seed = (float)c.type + 0.5f * fl.seed;
            flights_.push_back(fl);
        }
        // the seeds of the rows above step down
        {
            bool cleared[Game::H] = {};
            for (int y : rows) cleared[y] = true;
            float ns[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (cleared[y]) continue;
                for (int x = 0; x < Game::W; x++) ns[dst][x] = seeds_[y][x];
                dst--;
            }
            std::memcpy(seeds_, ns, sizeof(seeds_));
        }
        break;
    }
    case GameEvent::TopOut: {
        // the stack is lifted away, container by container, from the top
        overT_ = 0.f;
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            Flight fl;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            fl.p0 = fl.cur = vec3(p.x, p.y, -CELL_D);
            fl.p1 = fl.p0;
            fl.p2 = vec3(p.x * 1.3f, p.y + 26.f, -6.f - 4.f * hashf((uint64_t)(c.x * 31 + c.y)));
            fl.col = pieceColor(c.type);
            fl.kind = 1;
            fl.sizeK = 0.f;
            fl.dur = 1.7f;
            fl.delay = 0.12f * (c.y - top) + 0.25f * hashf((uint64_t)(c.x * 17 + c.y * 3));
            fl.seed = (float)c.type;
            flights_.push_back(fl);
        }
        spr_.on = false;
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The well: containers, the falling piece on its lifting beam, the ghost, the previews
// =====================================================================================================

void DocksUniverse::buildBoard(const Game& g) {
    auto cell = [&](const vec3& c, float half, int type, float seed, bool doors = true) {
        box(cells_, vec3(c.x, c.y, c.z - CELL_D * half / CELL_H), vec3(half, half, CELL_D * half / CELL_H), pieceColor(type), M_BOX,
            doors ? 0.f : 1.f, 0, 0, seed);
    };
    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            cell(p, CELL_H, c.type, seeds_[y][x]);
        }
    // the falling piece, exactly as it will lie
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            const vec3 w = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
            cell(w, CELL_H, p.type, 0.37f * i + 0.11f * p.type);
        }
        // the ghost: a thin outline of where it lands
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            auto has = [&](int x, int y) {
                for (auto& c : gc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            };
            const vec3 col = lerp(pieceColor(p.type), vec3(1.f), 0.25f) * 1.2f;
            const float wpx = std::max(1.f, 1.4f * height_ / 1080.f);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                const float h = 0.47f, z = 0.01f;
                if (!has(c[0], c[1] - 1)) line(cellLines_, vec3(w.x - h, w.y + h, z), vec3(w.x + h, w.y + h, z), wpx, col, 0.6f);
                if (!has(c[0], c[1] + 1)) line(cellLines_, vec3(w.x - h, w.y - h, z), vec3(w.x + h, w.y - h, z), wpx, col, 0.6f);
                if (!has(c[0] - 1, c[1])) line(cellLines_, vec3(w.x - h, w.y - h, z), vec3(w.x - h, w.y + h, z), wpx, col, 0.6f);
                if (!has(c[0] + 1, c[1])) line(cellLines_, vec3(w.x + h, w.y - h, z), vec3(w.x + h, w.y + h, z), wpx, col, 0.6f);
            }
        }
    }
    // the previews: small containers at the layout's anchors
    auto preview = [&](int type, vec2 center, float sc) {
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
            const float ox = cells[i][0] - (minx + maxx) * 0.5f, oy = cells[i][1] - (miny + maxy) * 0.5f;
            cell(vec3(center.x + ox * sc, center.y - oy * sc, 0.f), CELL_H * sc, type, 0.21f * i);
        }
    };
    if (!g.over()) {
        preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0));
        for (int i = 0; i < 3; i++) {
            const vec3 c = layout::nextCenter(i);
            if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i));
        }
    }
}

// The gantry's lifting beam over the falling piece: it spans the piece, a hanger reaches down to each column whose top
// is lower, hairline cables run up out of frame. On a lock it lets go and rises away.
void DocksUniverse::buildSpreader(const Game& g, float dt) {
    const vec3 beamCol(0.09f, 0.09f, 0.1f), capCol = lin(0xe0a020);
    const vec3 cableCol(0.6f, 0.6f, 0.58f);
    const float wpx = std::max(1.f, 1.05f * height_ / 1080.f);
    const bool holding = g.hasPiece() && !g.over() && overT_ < 0.f;
    if (spr_.rel >= 0.f) {
        spr_.rel += dt;
        if (spr_.rel > 0.9f || (holding && spr_.rel > 0.25f)) spr_.rel = -1.f;
    }
    if (holding && spr_.rel < 0.f) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        int minX = 99, maxX = -99, top = 99;
        for (auto& c : cells) { minX = std::min(minX, c[0]); maxX = std::max(maxX, c[0]); top = std::min(top, c[1]); }
        const vec3 tl = layout::cellPos(minX + dx, top + dy), br = layout::cellPos(maxX + dx, top + dy);
        const float x0 = tl.x - 0.48f, x1 = br.x + 0.48f, y = tl.y + 0.5f + 0.07f;
        // the extents follow the piece exactly when it moves; a rotation's change of shape eases in quickly
        if (!spr_.on) { spr_.x0 = x0; spr_.x1 = x1; spr_.y = y; spr_.on = true; }
        const float k = 1.f - std::exp(-dt * 30.f);
        const float cw = spr_.x1 - spr_.x0, nw = x1 - x0;
        const float ncx = (x0 + x1) * 0.5f;
        const float w = std::fabs(cw - nw) < 0.01f ? nw : lerpf(cw, nw, k);
        spr_.x0 = ncx - w * 0.5f;
        spr_.x1 = ncx + w * 0.5f;
        spr_.y = y;
        spr_.cx = ncx;
        // the hangers: down to every column whose top cell is lower than the piece's top
        spr_.hangCols = 0;
        for (int x = minX; x <= maxX && spr_.hangCols < 4; x++) {
            int colTop = 99;
            for (auto& c : cells)
                if (c[0] == x) colTop = std::min(colTop, c[1]);
            if (colTop == 99 || colTop == top) continue;
            spr_.hang[spr_.hangCols++] = (float)(x - minX) + 100.f * (float)(colTop - top);
        }
    } else if (!holding && spr_.rel < 0.f) {
        spr_.on = false;
    }
    if (!spr_.on) return;
    float y = spr_.y, a = 1.f;
    if (spr_.rel >= 0.f) {
        const float t = spr_.rel;
        y = spr_.relY + 0.6f * t + 26.f * t * t;
        a = 1.f - smoothstepf(0.4f, 0.9f, t);
    }
    if (a <= 0.01f) return;
    const float zc = -CELL_D, hd = CELL_D * 0.95f;
    box(boxes_, vec3(spr_.cx, y, zc), vec3((spr_.x1 - spr_.x0) * 0.5f, 0.06f, hd), beamCol, M_STEEL);
    // the yellow twistlock housings at its ends
    for (int s = -1; s <= 1; s += 2) {
        const float ex = s < 0 ? spr_.x0 + 0.1f : spr_.x1 - 0.1f;
        box(boxes_, vec3(ex, y + 0.02f, zc), vec3(0.1f, 0.085f, hd + 0.02f), capCol, M_STEEL);
    }
    if (spr_.rel < 0.f) {
        const float x0 = spr_.x0 + 0.48f;
        for (int i = 0; i < spr_.hangCols; i++) {
            const int col = (int)std::fmod(spr_.hang[i], 100.f);
            const int drop = (int)(spr_.hang[i] / 100.f);
            const float hx = x0 + col;
            for (float z : {0.0f, -2.f * CELL_D}) line(lines_, vec3(hx, y - 0.06f, z), vec3(hx, y - 0.06f - drop, z), wpx, cableCol, 0.7f);
            box(boxes_, vec3(hx, y - drop - 0.02f, zc), vec3(0.3f, 0.035f, hd * 0.9f), beamCol, M_STEEL);
        }
    }
    // the cables, up to the trolley out of frame
    const float topY = y + 34.f;
    for (int s = -1; s <= 1; s += 2) {
        const float ex = s < 0 ? spr_.x0 + 0.12f : spr_.x1 - 0.12f;
        for (float z : {0.02f, -2.f * CELL_D + 0.02f})
            line(lines_, vec3(ex, y + 0.06f, z), vec3(spr_.cx + s * 0.55f, topY, -CELL_D + (z > -0.1f ? 0.25f : -0.25f)), wpx, cableCol, 0.42f * a);
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float DocksUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) w += f.g[glyphIndex((unsigned char)s[i])].adv * k + track;
    return w - track;
}

void DocksUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const {
    if (!f.ok) return;
    const float k = size / f.px;
    const float baseline = y + f.capH * k;
    float pen = x;
    for (size_t i = 0; i < s.size(); i++) {
        const Glyph& g = f.g[glyphIndex((unsigned char)s[i])];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            const float v[6][4] = {{x0, y0, g.x0, g.y0}, {x1, y0, g.x1, g.y0}, {x1, y1, g.x1, g.y1},
                                   {x0, y0, g.x0, g.y0}, {x1, y1, g.x1, g.y1}, {x0, y1, g.x0, g.y1}};
            for (auto& q : v) out.insert(out.end(), q, q + 4);
        }
        pen += g.adv * k + track;
    }
}

void DocksUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha, float haloA) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", haloA);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void DocksUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

void DocksUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    // stencil-white lettering on a soft halo of the dusk, the accent in the sodium lamps' amber
    const vec3 ink(0.9f, 0.9f, 0.88f), halo = ugl::toSrgb(lerp(pal_.zen, pal_.fog, 0.3f)) * 0.45f;
    const vec3 accent = lerp(ugl::toSrgb(pal_.lampA), vec3(1.f), 0.15f);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                    {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }
    // the bonus gauge: a fine pale ring, its charge in amber, ticked like a dial
    if (vis > 0.01f && !bare_) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 120;
        const float th = std::max(1.2f, 1.5f * s), th2 = std::max(2.5f, 4.5f * s);
        auto seg = [&](float a0, float a1, float rr, float w, const vec3& col, float al) {
            const float s0 = std::sin(a0), c0 = std::cos(a0), s1 = std::sin(a1), c1 = std::cos(a1);
            const float ri = rr - w * 0.5f, ro = rr + w * 0.5f;
            const float pts[4][2] = {{c.x + s0 * ri, c.y - c0 * ri}, {c.x + s0 * ro, c.y - c0 * ro},
                                     {c.x + s1 * ro, c.y - c1 * ro}, {c.x + s1 * ri, c.y - c1 * ri}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {pts[k][0], pts[k][1], col.x, col.y, col.z, al};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
        };
        const float dimk = vis * (1.f - 0.5f * dimT_);
        for (int i = 0; i < N; i++) seg(TAU * i / N, TAU * (i + 1) / N, r, th, ink, (g.bonusReady() ? 0.5f : 0.22f) * dimk);
        for (int i = 0; i < Game::BONUS_LINES; i++) {
            const float a = TAU * i / Game::BONUS_LINES;
            seg(a - 0.012f, a + 0.012f, r - th2, th2 * 0.9f, ink, 0.35f * dimk);
        }
        flushShapes();
        for (int i = 0; i < N; i++) {
            if ((i + 0.5f) / N > fill) break;
            seg(TAU * i / N, TAU * (i + 1) / N, r, th2, accent, 0.92f * dimk);
        }
        flushShapes();
    }
    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha) * vis;
        q.clear();
        auto place = [&](const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(font_, text, size, track);
            textQuads(font_, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        auto upper = [](std::string t) {
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            return t;
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(upper(h.text), base * 0.72f, base * 0.28f, base * 0.06f);
            drawText(font_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f) * 0.75f * vis);
            continue;
        case HUD_SECTION:
            place(upper(h.text), base * 0.8f, base * 0.3f, -base * 0.04f);
            drawText(font_, q, accent, halo, saturate(0.3f + h.alpha * 1.7f) * vis);
            continue;
        case HUD_TITLE:
            place(h.text, base * 1.1f, base * 0.02f, -base * 0.1f);
            drawText(font_, q, ink, halo, saturate(h.alpha * 1.3f) * vis);
            continue;
        case HUD_INFO:
            place(upper(h.text), base * 0.64f, base * 0.22f);
            drawText(font_, q, ink, halo, saturate(alpha * 1.2f) * 0.72f);
            continue;
        case HUD_LABEL:
            place(upper(h.text), base * 0.66f, base * 0.36f);
            drawText(font_, q, ink, halo, saturate(alpha * 1.2f) * 0.6f);
            continue;
        case HUD_VALUE:
            place(h.text, base * 1.0f, base * 0.08f, -base * 0.04f);
            drawText(font_, q, ink, halo, alpha * 0.95f);
            continue;
        case HUD_NOTICE:
            place(upper(h.text), base * 0.9f, base * 0.2f);
            drawText(font_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(h.text, base * 0.84f, base * 0.1f);
            drawText(font_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(upper(h.text), base * 1.0f, base * 0.34f, -base * 0.1f);
            drawText(font_, q, ink, halo, alpha, 0.6f);
            continue;
        case HUD_TOAST_SUB:
            place(h.text, base * 0.95f, base * 0.1f);
            drawText(font_, q, accent, halo, alpha * 0.9f, 0.6f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(upper(h.text), base * 0.9f, base * 0.26f);
                drawText(font_, q, ink, halo, alpha, 0.7f);
            } else {
                place(h.text, base * 0.95f, base * 0.04f);
                drawText(font_, q, h.accent ? accent : ink, halo, alpha, 0.7f);
            }
            continue;
        default:
            place(h.text, base * 0.78f, base * 0.06f);
            drawText(font_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Drawing
// =====================================================================================================

void DocksUniverse::drawBoxes(GLuint vao, GLuint vbo, const std::vector<Box>* data, size_t count, const mat4& vp, float clipY, bool upload) {
    if (!count) return;
    (void)vp;
    (void)clipY;
    glBindVertexArray(vao);
    if (upload && data) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(Box)), data->data(), GL_STREAM_DRAW);
    }
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, (GLsizei)count);
}

void DocksUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    // ---- A new variant eases in.
    {
        const float k = 1.f - std::exp(-dt * 1.2f);
        Palette& P = pal_;
        const Palette& T = palT_;
        lerpPal(P.zen, T.zen, k); lerpPal(P.mid, T.mid, k); lerpPal(P.hor, T.hor, k); lerpPal(P.glow, T.glow, k);
        lerpPal(P.sun, T.sun, k); lerpPal(P.fog, T.fog, k); lerpPal(P.amb, T.amb, k); lerpPal(P.ground, T.ground, k);
        lerpPal(P.key, T.key, k); lerpPal(P.water, T.water, k); lerpPal(P.lampA, T.lampA, k); lerpPal(P.lampB, T.lampB, k);
        lerpPal(P.quay, T.quay, k); lerpPal(P.crane, T.crane, k); lerpPal(P.crane2, T.crane2, k);
        for (int i = 0; i < 7; i++) lerpPal(P.piece[i], T.piece[i], k);
        for (int i = 0; i < 3; i++) lerpPal(P.hull[i], T.hull[i], k);
        P.sunAz = lerpf(P.sunAz, T.sunAz, k); P.sunEl = lerpf(P.sunEl, T.sunEl, k); P.stars = lerpf(P.stars, T.stars, k);
        P.fogD = lerpf(P.fogD, T.fogD, k); P.moon = lerpf(P.moon, T.moon, k); P.haze = lerpf(P.haze, T.haze, k);
        P.lampK = lerpf(P.lampK, T.lampK, k);
    }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 0.9f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    time_ += dt;
    worldTime_ += wdt;
    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    if (music.hasSong) beat_ = (float)music.beat;
    else beat_ += wdt * 2.f;
    const float spb = 60.f / std::max(60.f, music.bpm);
    dropT_ += dt;
    tetrisT_ += dt;
    float festT = 0.f;
    if (dropMode_) festT = smoothstepf(0.f, 2.f * spb, dropT_) * (1.f - 0.5f * smoothstepf(16.f * spb, 64.f * spb, dropT_));
    fest_ = approach(fest_, festT, festT > fest_ ? 1.5f : 0.25f, dt);
    // (the swell eases in over a beat or so, then dies away slowly: no sudden jump of the light)
    swellPeak_ = approach(swellPeak_, 0.f, 0.35f, dt);
    swell_ = approach(swell_, swellPeak_, 2.f, dt);
    refreshK_ = approach(refreshK_, 0.f, 0.5f, dt);
    const float half = beat_ * 0.5f;
    const int halfI = (int)std::floor(half);
    const float ph = half - std::floor(half);
    const bool newHalf = halfI != lastHalf_;
    lastHalf_ = halfI;
    float kick = kickIn_;
    kickIn_ = 0.f;
    // how busy the port is (the cranes' pace)
    const float pace = (0.15f + 0.85f * wm + 0.6f * wp + 0.6f * fest_) * (1.f - 0.6f * slow_);

    // ---- The cranes: a job (pick from the quay, carry to the ship, set down, back), a step every two beats.
    for (size_t i = 0; i < cranes_.size(); i++) {
        Crane& c = cranes_[i];
        if (c.raise > 0.5f) continue;
        if (c.tT < 1.f) {
            c.tT = std::min(1.f, c.tT + wdt / c.tDur);
            const float e = smoothstepf(0.f, 1.f, c.tT);
            c.trolley = lerpf(c.tFrom, c.tTo, e);
            c.hoist = lerpf(c.hFrom, c.hTo, e);
        } else if (newHalf && (int)((halfI + (int)i * 3) % 3) == 0 && rng_.uniform() < 0.25f + 0.75f * saturate(pace - 0.2f)) {
            // the next step of its cycle
            c.tFrom = c.trolley;
            c.hFrom = c.hoist;
            c.tT = 0.f;
            c.tDur = std::clamp(4.f * spb / std::max(0.3f, pace), 1.0f, 6.f);
            const bool overShip = c.trolley > 0.45f;
            if (c.hoist < 0.5f) {
                // at the top: travel, or lower
                if (c.loaded != overShip) { c.tTo = overShip ? 0.2f + 0.08f * hashf(i + halfI) : 0.62f + 0.12f * hashf(i * 5 + halfI); c.hTo = 0.f; }
                else { c.tTo = c.trolley; c.hTo = 1.f; c.tDur *= 0.7f; }
            } else {
                // at the bottom: take or leave the box, hoist up
                c.loaded = !c.loaded;
                if (c.loaded) c.colorIdx = (int)(hashf(i * 13 + halfI) * 10.f);
                c.tTo = c.trolley;
                c.hTo = 0.f;
                c.tDur *= 0.7f;
            }
        }
        c.blink = approach(c.blink, 0.f, 6.f, dt);
        c.beacon = 0.f;
    }
    if (kick > 0.25f && wp > 0.4f && !cranes_.empty()) cranes_[rng_.irange(0, (int)cranes_.size() - 1)].blink = 1.f;

    // ---- The ships: full ones leave, empty ones come in.
    for (int i = 0; i < 2; i++) {
        Ship& sh = ships_[i];
        for (float& t : sh.landT) t += wdt;
        const float out = i == 0 ? -1.f : 1.f;
        const float berth = shipBerthX(i);
        sh.wake = approach(sh.wake, 0.f, 0.5f, dt);
        if (sh.state == 0) {
            bool full = sh.reserved >= (int)sh.order.size();
            if (full) {
                bool pending = false;
                for (const Flight& fl : flights_)
                    if (fl.ship == i) pending = true;
                // one ship at a time casts off: the other stays at its berth
                if (!pending && ships_[1 - i].state == 0) {
                    sh.wait += wdt;
                    if (sh.wait > 4.f) { sh.state = 1; sh.t = 0.f; sh.wait = 0.f; }
                }
            }
        } else if (sh.state == 1) {
            sh.t += wdt / 18.f;
            const float e = sh.t * sh.t;
            sh.x = berth + out * 160.f * e;
            sh.wake = std::max(sh.wake, saturate(sh.t * 3.f));
            if (sh.t >= 1.f) { sh.state = 3; sh.wait = 0.f; }
        } else if (sh.state == 3) {
            sh.wait += wdt;
            if (sh.wait > 2.f) newShip(i, false);
        } else if (sh.state == 2) {
            sh.t += wdt / 20.f;
            const float e = 1.f - (1.f - std::min(1.f, sh.t)) * (1.f - std::min(1.f, sh.t));
            sh.x = berth + out * 150.f * (1.f - e);
            sh.wake = std::max(sh.wake, 1.f - e);
            if (sh.t >= 1.f) { sh.state = 0; sh.x = berth; }
        }
    }

    // ---- The cleared containers on their way.
    for (Flight& fl : flights_) {
        if (fl.kind == 1) {
            // game over: lifted out of frame
            fl.t += dt;
            const float s = saturate((fl.t - fl.delay) / fl.dur);
            const float e = s * s * (1.5f - 0.5f * s);
            fl.cur = lerp(fl.p0, fl.p2, e);
            continue;
        }
        fl.t += fl.t < LIFT_T + fl.delay ? dt : wdt;
        const float tt = fl.t - fl.delay;
        if (tt <= 0.f) { fl.cur = fl.p0; continue; }
        if (tt < LIFT_T) {
            // swiftly up and back, out of the well
            const float s = tt / LIFT_T;
            const float e = 1.f - (1.f - s) * (1.f - s) * (1.f - s);
            fl.cur = vec3(lerpf(fl.p0.x, fl.p1.x, e), lerpf(fl.p0.y, fl.p1.y, smoothstepf(0.f, 0.6f, s) * 0.7f + 0.3f * e),
                          lerpf(fl.p0.z, fl.p1.z, e * e));
            fl.sizeK = 0.f;
        } else {
            // across the basin, high on the cables, then set down on the deck
            const float s = saturate((tt - LIFT_T) / fl.dur);
            const float e = 1.f - std::pow(1.f - s, 2.2f); // leaves at once, sets down gently
            const vec3 a = fl.p1, d = fl.p2;
            const vec3 b = vec3(lerpf(a.x, d.x, 0.25f), std::max(a.y, d.y) + 5.f, lerpf(a.z, d.z, 0.3f));
            const vec3 c = vec3(d.x, d.y + 6.f, lerpf(a.z, d.z, 0.85f));
            const float u = 1.f - e;
            fl.cur = a * (u * u * u) + b * (3.f * u * u * e) + c * (3.f * u * e * e) + d * (e * e * e);
            fl.sizeK = smoothstepf(0.55f, 0.95f, s);
            if (s >= 1.f) {
                if (fl.ship >= 0 && fl.slot >= 0 && ships_[fl.ship].state == 0) {
                    ships_[fl.ship].slot[fl.slot] = (signed char)std::clamp((int)fl.seed, 0, 6);
                    ships_[fl.ship].landT[fl.slot] = 0.f;
                }
                fl.t = 1e9f;
            }
        }
    }
    flights_.erase(std::remove_if(flights_.begin(), flights_.end(),
                                  [](const Flight& fl) { return fl.t > 1e8f || (fl.kind == 1 && fl.t > fl.delay + fl.dur); }),
                   flights_.end());
    if (!bare_) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- Lights: the masts on by level, all on (in a sequence from the well) on the drop.
    const float lampLevel = pal_.lampK * (0.55f + 0.25f * wm + 0.2f * wp + 0.35f * fest_) * (1.f + 0.25f * slow_);
    // the light cones in the haze: none when calm, faint at mid, full at a peak and on the drop
    const float coneK = (0.15f * wm + 0.5f * wp + 0.8f * fest_ + 0.5f * swell_) * (1.f + 0.6f * slow_);
    cones_.clear();
    pointP_.clear();
    pointC_.clear();
    sprites_.clear();
    for (Lamp& l : lamps_) {
        float want = l.group == 0 ? 1.f : l.group == 1 ? smoothstepf(0.3f, 0.9f, lv_) : smoothstepf(1.2f, 1.8f, lv_);
        if (dropMode_ && dropT_ > l.order) want = 1.f;
        l.on = approach(l.on, want, want > l.on ? 2.5f : 0.6f, dt);
        float k = l.k * l.on * lampLevel;
        if (dropT_ > l.order) k *= 1.f + 0.9f * swell_ * std::exp(-(dropT_ - l.order) * 1.5f);
        if (tetrisT_ > l.order * 0.5f && tetrisT_ < 4.f) k *= 1.f + 0.7f * std::exp(-(tetrisT_ - l.order * 0.5f) * 2.f);
        const float rr = l.r * (1.f + 0.6f * slow_);
        sprite(l.p, rr, l.col, k);
        if (l.range > 0.f && k > 0.05f) {
            pointP_.push_back(vec4(l.p, l.range));
            pointC_.push_back(vec4(l.col, k * 1.6f));
            if (coneK * k > 0.01f) {
                const vec3 base(l.p.x, Y_F + 0.2f, l.p.z + 6.f);
                cones_.push_back({{l.p.x, l.p.y, l.p.z, 0.8f}, {base.x, base.y, base.z, 9.f}, {l.col.x, l.col.y, l.col.z, 0.14f * coneK * k}});
            }
        }
    }
    // the cranes' lamps: floods under the boom (lighting the decks), a red beacon at the apex, the trolley's lamp
    for (size_t i = 0; i < cranes_.size(); i++) {
        const Crane& c = cranes_[i];
        const float fk = pal_.lampK * (0.3f + 0.45f * wm + 0.35f * wp + 0.35f * fest_);
        for (int k = 0; k < 3 && c.raise < 0.5f; k++) {
            const float z = lerpf(CRANE_SEA + 2.f, BOOM_TIP + 3.f, k / 2.f);
            const vec3 lp(c.x + (k == 1 ? 0.f : 1.8f * (k ? 1.f : -1.f)), BOOM_Y - 0.9f, z);
            sprite(lp, 0.9f, pal_.lampB, fk * 0.9f);
            if (coneK > 0.01f) {
                const vec3 base(lp.x, DECK_Y + 2.f, lp.z + 1.5f);
                cones_.push_back({{lp.x, lp.y, lp.z, 0.35f}, {base.x, base.y, base.z, 5.f}, {pal_.lampB.x, pal_.lampB.y, pal_.lampB.z, 0.2f * coneK * fk}});
            }
        }
        if (c.raise < 0.5f) pointP_.push_back(vec4(vec3(c.x, BOOM_Y - 2.f, SHIP_Z + 3.f), 26.f));
        if (c.raise < 0.5f) pointC_.push_back(vec4(pal_.lampB, fk * 1.6f));
        // the beacon: on its own slow clock when calm, on the two-beat grid at a peak
        const float own = 0.5f + 0.5f * std::sin(worldTime_ * 1.3f + i * 1.7f);
        if (c.raise > 0.5f) {
            // the raised boom's tip lamp
            const float ang = c.raise * 1.32f;
            const vec3 tip(c.x, BOOM_Y + std::sin(ang) * (BOOM_TIP - CRANE_SEA - 1.f), CRANE_SEA + 1.f + std::cos(ang) * (BOOM_TIP - CRANE_SEA - 1.f));
            sprite(tip + vec3(0, 0.6f, 0), 0.9f, vec3(1.f, 0.06f, 0.03f), 1.2f * (0.35f + 0.65f * own * own));
            sprite(vec3(c.x, CRANE_APEX + 0.5f, (CRANE_SEA + CRANE_LAND) * 0.5f + 1.f), 0.9f, vec3(1.f, 0.06f, 0.03f), 1.2f * (0.35f + 0.65f * own * own));
            continue;
        }
        const float grid = (((halfI + (int)i) % 2) == 0 ? 1.f : 0.f) * (1.f - smoothstepf(0.15f, 0.45f, ph));
        const float beacon = lerpf(0.35f + 0.65f * own * own, grid, wp);
        sprite(vec3(c.x, CRANE_APEX + 0.5f, (CRANE_SEA + CRANE_LAND) * 0.5f + 1.f), 0.9f, vec3(1.f, 0.06f, 0.03f), 1.4f * beacon);
        const float tz = lerpf(BOOM_BACK + 12.f, BOOM_TIP + 2.f, c.trolley);
        sprite(vec3(c.x, BOOM_Y - 1.1f, tz + 1.4f), 0.7f, lerp(pal_.lampA, vec3(1.f), c.blink), 0.6f + 2.2f * c.blink);
    }
    // the ships' lamps: deck floods on the house, the mast's lights
    for (int i = 0; i < 2; i++) {
        const Ship& sh = ships_[i];
        if (sh.state == 3) continue;
        const float s = i == 0 ? 1.f : -1.f;
        const float xs = sh.x - s * SHIP_LEN * 0.5f;
        const float deck = (0.5f + 0.3f * wm + 0.3f * wp + 0.4f * fest_) * pal_.lampK;
        const float tet = tetrisT_ < 6.f ? std::exp(-tetrisT_ * 0.5f) : 0.f;
        sprite(vec3(xs + s * 9.8f, DECK_Y + 8.8f, SHIP_Z + 4.f), 0.8f, pal_.lampA, deck * (1.f + tet));
        sprite(vec3(xs + s * 9.8f, DECK_Y + 8.8f, SHIP_Z - 4.f), 0.8f, pal_.lampA, deck * (1.f + tet));
        sprite(vec3(xs + s * 6.f, DECK_Y + 12.5f, SHIP_Z), 0.5f, vec3(1.f), 0.9f);
        sprite(vec3(xs + s * (SHIP_LEN - 1.5f), DECK_Y + 2.6f, SHIP_Z), 0.45f, vec3(1.f), 0.7f);
        pointP_.push_back(vec4(vec3(xs + s * 11.f, DECK_Y + 9.f, SHIP_Z + 3.f), 22.f));
        pointC_.push_back(vec4(pal_.lampA, deck * (0.8f + 1.2f * tet)));
    }

    // ---- Geometry.
    boxes_.clear();
    cells_.clear();
    lines_.clear();
    cellLines_.clear();
    for (const Crane& c : cranes_) buildCrane(boxes_, c, true);
    for (int i = 0; i < 2; i++) buildShip(boxes_, i);
    // the cranes' hoist cables
    {
        const vec3 cc(0.35f, 0.35f, 0.34f);
        for (const Crane& c : cranes_) {
            if (c.raise > 0.5f) continue;
            const float tz = lerpf(BOOM_BACK + 12.f, BOOM_TIP + 2.f, c.trolley);
            const float sy = craneHook(c);
            for (int s = -1; s <= 1; s += 2) line(lines_, vec3(c.x + s * 1.f, BOOM_Y - 0.9f, tz), vec3(c.x + s * 1.f, sy + 0.1f, tz), 1.f, cc, 0.5f);
            // the mooring lines
        }
        for (int i = 0; i < 2; i++) {
            const Ship& sh = ships_[i];
            if (sh.state != 0) continue;
            const float s = i == 0 ? 1.f : -1.f;
            const float xs = sh.x - s * SHIP_LEN * 0.5f;
            for (float u : {1.f, 3.f, SHIP_LEN - 4.f, SHIP_LEN - 2.f}) {
                const vec3 a(xs + s * u, DECK_Y + 0.6f, SHIP_Z - SHIP_HALF_B + 0.3f);
                const vec3 b(xs + s * (u + (u < 30.f ? -6.f : 6.f)), Y_F + 0.3f, Z_FE - 1.f);
                (void)a; (void)b; // (mooring lines read as stray dashes at this distance)
            }
        }
    }
    // the cleared containers on their cables
    {
        const vec3 cc(0.55f, 0.55f, 0.53f);
        const float wpx = std::max(1.f, 1.0f * height_ / 1080.f);
        for (const Flight& fl : flights_) {
            const float tt = fl.t - fl.delay;
            if (fl.kind == 1 && tt < 0.f) continue;
            if (bare_ && (fl.kind == 1 || tt < LIFT_T)) continue; // the world alone: none still in or by the well
            const int type = std::clamp((int)fl.seed, 0, 6);
            const vec3 col = pieceColor(type);
            const vec3 hc(CELL_H, CELL_H, CELL_D);
            const vec3 hs(1.27f, 0.47f, 0.46f);
            const vec3 h = lerp(hc, hs, fl.sizeK);
            box(boxes_, fl.cur, h, col, M_BOX, fl.sizeK > 0.5f ? 1.f : 0.f, 0, 0, fl.seed);
            // its cable: up out of frame near the well, up to a crane's boom over the basin
            // (out over the basin, the cable is taken up to the booms of the cranes across)
            const float far = smoothstepf(-24.f, -36.f, fl.cur.z);
            const float topY = std::max(fl.cur.y + 1.5f, lerpf(fl.cur.y + 30.f, BOOM_Y - 0.8f, far));
            const float a = fl.kind == 1 ? 1.f : smoothstepf(0.f, 0.08f, tt);
            const vec3 top(fl.cur.x, fl.cur.y + h.y, fl.cur.z);
            for (int s = -1; s <= 1; s += 2)
                line(lines_, top + vec3(s * h.x * 0.6f, 0.f, 0.f), vec3(fl.cur.x + s * 0.25f, topY, fl.cur.z), wpx, cc, 0.24f * a);
        }
    }
    if (!bare_) {
        buildBoard(game);
        buildSpreader(game, dt);
    }

    // ---- The lights for shading: the brightest, nearest first.
    {
        std::vector<int> idx(pointP_.size());
        for (size_t i = 0; i < idx.size(); i++) idx[i] = (int)i;
        auto score = [&](int i) {
            const vec4& p = pointP_[i];
            const float d = std::fabs(p.x) * 0.6f + std::fabs(p.z + 70.f) * 0.5f;
            return pointC_[i].w * p.w / (20.f + d);
        };
        std::sort(idx.begin(), idx.end(), [&](int a, int b) { return score(a) > score(b); });
        std::vector<vec4> P, C;
        for (size_t i = 0; i < idx.size() && i < 16; i++) { P.push_back(pointP_[idx[i]]); C.push_back(pointC_[idx[i]]); }
        pointP_.swap(P);
        pointC_.swap(C);
    }

    // ---- Uniforms shared by the passes.
    const Palette& P = pal_;
    const vec3 sunDir = normalize(vec3(std::sin(P.sunAz) * std::cos(P.sunEl), std::sin(P.sunEl) + 0.001f, -std::cos(P.sunAz) * std::cos(P.sunEl)));
    const float glowK = (0.72f + 0.18f * wm + 0.15f * wp + 0.4f * fest_ + 0.3f * swell_) * (1.f - 0.4f * slow_);
    const float fogD = P.fogD * (1.f + 0.7f * (1.f - wm) - 0.15f * wp) * (1.f + 2.2f * slow_);
    const float haze = std::min(1.5f, P.haze * (1.f + 0.4f * (1.f - wm)) + 0.35f * (1.f - wm) + 0.9f * slow_);
    const float sat = 1.f + 0.1f * wp + 0.18f * fest_;
    const vec3 keyDir = normalize(vec3(-0.25f, 0.8f, 0.55f));
    const float keyK = 0.9f + 0.06f * wm + 0.08f * wp + 0.1f * fest_;
    const vec3 fogCol = lerp(P.fog, lerp(P.fog, vec3(0.55f, 0.6f, 0.68f), 0.5f) * 0.6f, slow_);
    auto common = [&](GLuint p) {
        set3f(p, "uEye", eye_);
        set3f(p, "uZen", P.zen);
        set3f(p, "uMid", P.mid);
        set3f(p, "uHor", P.hor * (0.9f + 0.1f * glowK));
        set3f(p, "uGlow", P.glow * glowK * 0.55f);
        set3f(p, "uSunDir", sunDir);
        set3f(p, "uFogCol", fogCol);
        set1f(p, "uFogD", fogD);
        set1f(p, "uHaze", haze);
        set1f(p, "uTime", worldTime_);
        set1f(p, "uSat", sat);
    };
    auto lighting = [&](GLuint p) {
        set3f(p, "uAmbSky", P.amb);
        set3f(p, "uAmbGround", P.ground);
        set3f(p, "uKeyDir", keyDir);
        set3f(p, "uKeyCol", P.key * keyK);
        set1f(p, "uKeyFar", 0.2f);
        set3f(p, "uSunCol", P.sun * glowK);
        const int n = (int)pointP_.size();
        set1i(p, "uNL", n);
        if (n) {
            glUniform4fv(ugl::U(p, "uLP"), n, &pointP_[0].x);
            glUniform4fv(ugl::U(p, "uLC"), n, &pointC_[0].x);
        }
        set1f(p, "uWaterY", Y_W);
        set3f(p, "uWinCol", lerp(P.lampA, vec3(1.f, 0.85f, 0.6f), 0.5f) * (0.45f + 0.4f * wm + 0.2f * wp + 0.3f * fest_));
        set3f(p, "uQuayLine", lin(0xc8a030) * 0.9f);
        set1f(p, "uBay", bare_ ? 0.f : 1.f);
        set1f(p, "uDeckLights", saturate(0.3f + 0.4f * wm + 0.3f * wp + (tetrisT_ < 4.f ? 0.5f : 0.f)));
    };
    auto drawSky = [&](const mat4& vp, bool mirror) {
        glUseProgram(progSky_);
        common(progSky_);
        setMat(progSky_, "uInvVP", inverse(vp));
        set1f(progSky_, "uMirror", mirror ? 1.f : 0.f);
        set1f(progSky_, "uStars", P.stars * (1.f - 0.6f * slow_));
        set1f(progSky_, "uMoon", P.moon * (1.f - 0.7f * slow_));
        set3f(progSky_, "uMoonDir", normalize(vec3(0.4f, 0.21f, -1.f)));
        set1f(progSky_, "uSeed", seedF_);
        set1f(progSky_, "uShoreLights", 0.5f + 0.3f * wm + 0.3f * fest_);
        set1f(progSky_, "uClouds", variant_ == 4 ? 0.9f : 0.55f);
        set3f(progSky_, "uShore", lerp(P.zen, P.fog, 0.5f) * 0.55f);
        set3f(progSky_, "uTown", lerp(P.lampA, vec3(1.f, 0.9f, 0.7f), 0.3f) * 1.4f);
        ugl::drawFullscreen();
    };
    // the sky, drawn last among the opaque things and only where nothing stands (at the far plane, behind the depth
    // test): most of the screen is cranes, ships and quay, and the sky's clouds and hills are costly
    auto drawSkyBehind = [&](const mat4& vp, bool mirror) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        glDepthRange(1.0, 1.0);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        drawSky(vp, mirror);
        glDepthRange(0.0, 1.0);
        glDepthFunc(GL_LESS);
    };
    auto drawSolids = [&](const mat4& vp, float clipY) {
        glUseProgram(progBox_);
        common(progBox_);
        lighting(progBox_);
        setMat(progBox_, "uVP", vp);
        set1f(progBox_, "uClipY", clipY);
        set1f(progBox_, "uStackA", 0.f);
        if (staticDirty_) {
            // nearest first, so that the depth test rejects most of what is hidden before it is shaded
            std::vector<Box> sorted = static_;
            auto dist2 = [&](const Box& b) {
                const float dx = b.c[0] - eye_.x, dy = b.c[1] - eye_.y, dz = b.c[2] - eye_.z;
                return dx * dx + dy * dy + dz * dz;
            };
            std::sort(sorted.begin(), sorted.end(), [&](const Box& a, const Box& b) { return dist2(a) < dist2(b); });
            glBindBuffer(GL_ARRAY_BUFFER, staticVbo_);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sorted.size() * sizeof(Box)), sorted.data(), GL_STATIC_DRAW);
            staticCount_ = sorted.size();
            staticDirty_ = false;
        }
        drawBoxes(staticVao_, staticVbo_, nullptr, staticCount_, vp, clipY, false);
        drawBoxes(boxVao_, boxVbo_, &boxes_, boxes_.size(), vp, clipY, true);
    };
    auto drawSpriteList = [&](const mat4& vp, float clipY, bool mirror) {
        if (sprites_.empty()) return;
        glUseProgram(progSprite_);
        common(progSprite_);
        setMat(progSprite_, "uVP", vp);
        set1f(progSprite_, "uClipY", clipY);
        set1f(progSprite_, "uMirror", mirror ? 1.f : 0.f);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sprites_.size() * sizeof(Sprite)), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    };
    auto drawConeList = [&](const mat4& vp, float clipY) {
        if (cones_.empty()) return;
        glUseProgram(progCone_);
        common(progCone_);
        setMat(progCone_, "uVP", vp);
        set1f(progCone_, "uClipY", clipY);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(cones_.size() * sizeof(Line)), cones_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cones_.size());
    };
    auto drawLineList = [&](const std::vector<Line>& L, const mat4& vp, int w, int h) {
        if (L.empty()) return;
        glUseProgram(progLine_);
        common(progLine_);
        setMat(progLine_, "uVP", vp);
        set2f(progLine_, "uRes", (float)w, (float)h);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(L.size() * sizeof(Line)), L.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)L.size());
    };

    // ---- The reflection: the port mirrored in the water (half resolution).
    const mat4 vpM = vp_ * mirror_;
    {
        reflT_.bind();
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT); // the mirror flips the winding
        drawSolids(vpM, Y_W);
        drawSkyBehind(vp_, true);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        drawSpriteList(vpM, Y_W, true);
        drawConeList(vpM, Y_W);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    // ---- The scene (multisampled).
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    // the water writes its depth
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glUseProgram(progWater_);
    common(progWater_);
    setMat(progWater_, "uInvVP", invVp_);
    setMat(progWater_, "uVP", vp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, reflT_.tex);
    set1i(progWater_, "uRefl", 0);
    set1f(progWater_, "uWaterY", Y_W);
    set1f(progWater_, "uRipple", (1.f + 0.3f * wm + 0.25f * fest_) * (1.f - 0.5f * slow_));
    set1f(progWater_, "uReflK", 0.95f);
    set3f(progWater_, "uDeep", P.water * 0.6f);
    set3f(progWater_, "uAmbSky", P.amb);
    {
        float wk[4] = {ships_[0].x - SHIP_LEN * 0.4f, ships_[0].wake, ships_[1].x + SHIP_LEN * 0.4f, ships_[1].wake};
        glUniform2fv(ugl::U(progWater_, "uWake"), 2, wk);
    }
    ugl::drawFullscreen();
    glDepthFunc(GL_LESS);
    // the solids (the scene's alpha is the stack's, for the bridge)
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    drawSolids(vp_, -1e9f);
    // the stack
    if (!cells_.empty()) {
        set1f(progBox_, "uStackA", 1.f);
        drawBoxes(boxVao_, boxVbo_, &cells_, cells_.size(), vp_, -1e9f, true);
    }
    drawSkyBehind(vp_, false);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glDepthMask(GL_FALSE);
    // the well's soft shade, behind the stack
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // (deeper under a bright sky, so the containers stand out from the port)
    const float skyLum = 0.2126f * P.hor.x + 0.7152f * P.hor.y + 0.0722f * P.hor.z;
    const float bright = smoothstepf(0.35f, 0.6f, skyLum);
    if (!bare_) {
        glUseProgram(progVeil_);
        setMat(progVeil_, "uVP", vp_);
        set4f(progVeil_, "uRect", -5.4f, Y_Q, 5.4f, 11.5f);
        set1f(progVeil_, "uZ", -2.f * CELL_D - 0.1f);
        set3f(progVeil_, "uTint", P.zen * 0.3f);
        set1f(progVeil_, "uA", 0.32f + 0.08f * bright);
        set1f(progVeil_, "uHorY", EYE_Y);
        glBindVertexArray(quadVao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    // the lamps, the cables, the ghost
    glBlendFunc(GL_ONE, GL_ONE);
    drawSpriteList(vp_, -1e9f, false);
    drawConeList(vp_, -1e9f);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawLineList(lines_, vp_, width_, height_);
    glDisable(GL_DEPTH_TEST);
    drawLineList(cellLines_, vp_, width_, height_);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // ---- Resolve, the glow, the frame.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    set1f(progDown_, "uThr", 0.35f);
    ugl::drawFullscreen();
    glUseProgram(progBlur_);
    set1i(progBlur_, "uSrc", 0);
    for (int i = 0; i < 2; i++) {
        const float st = 1.f + i;
        bloomB_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
        set2f(progBlur_, "uDir", st / bloomA_.w, 0.f);
        ugl::drawFullscreen();
        bloomA_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
        set2f(progBlur_, "uDir", 0.f, st / bloomA_.h);
        ugl::drawFullscreen();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    set1i(progFinal_, "uBloom", 1);
    glActiveTexture(GL_TEXTURE0);
    set1f(progFinal_, "uBloomK", (0.55f + 0.1f * wp + 0.2f * fest_) * (1.f + 0.5f * slow_));
    set1f(progFinal_, "uExposure", 1.f);
    set1f(progFinal_, "uHalo", 0.16f + 0.06f * bright);  // a halo around the containers (a little more under a bright sky)
    set1f(progFinal_, "uSatF", 1.f + 0.06f * wp + 0.1f * fest_);
    set1f(progFinal_, "uSlow", slow_ * 0.45f);
    set3f(progFinal_, "uSlowTint", vec3(0.8f, 0.9f, 1.05f));
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
    (void)ph;
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_docks() {
    static const UniverseEntry e{
        "DOCKS", "a container port at dusk in 3D: the pieces are containers lowered by a gantry, cleared rows are craned onto ships",
        [] { return std::unique_ptr<Universe>(new DocksUniverse()); },
        // A big, busy world: suits driving, rhythmic songs best; fits most.
        [](const SongLook& l) { return 0.75f + 0.2f * l.density + 0.15f * l.bassWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
