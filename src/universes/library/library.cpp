// LIBRARY: the blocks are books, and every cleared row is put away on the shelves of a growing personal library.
//
// The pieces are books standing in the well, one per cell, their spines toward us: cloth bindings in a clean color
// per piece, gilt bands and a label on the rounded spine, the page block showing on top; the well is a tall bookcase
// of dark wood. When a row clears, its ten books slide out of the well and float calmly across the room into the gaps
// that open for them on the shelves, sorted: by color in rainbow order, the tallest first in each color, the books
// after them sliding along (and over to the next shelf) to make room. Every book on the shelves is a cleared cell:
// N lines, 10 N books. They stay for the game.
//
// It starts as a cosy study with one empty bookcase (the desk and its green lamp under the window, a fire, an
// armchair, a globe, a longcase clock). Once that bookcase is well filled (S_ROOM), the study's walls fall away into a
// tall long room: six empty bookcases standing ready to the right of the study's, high arched windows, reading tables,
// chandeliers. When those are full, more bookcases are carried in, left of the study, then down the side walls. The
// camera eases back a little to keep the collection in view.
//
// The song: dust motes drift in the lamplight; the lamps follow the music (brighter at a peak, lifted by the kicks);
// a real drop (the HUD's section, else the music's pace) sends a soft light running along the collection's spines
// and warms the lamps; a Tetris sends one soft light along them. Slow motion: moonlight. Game over: the stack's books
// tumble out of the well.
//
// Layout contract: the board is drawn through the reference camera raised a little and shifted back down (the
// board plane keeps its pixels, the books' tops show); the library has its own camera behind it.
#include "universes/library/library.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "layout.hpp"
#include "universes/library/shaders.hpp"
#include "universes/library/world.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

using namespace lib_world;

namespace {

// Libre Baskerville (Regular, Italic), SIL Open Font License, see OFL-LibreBaskerville.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/library/LibreBaskerville-Regular.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/library/LibreBaskerville-Italic.ttf.inc"
};

namespace shaders = library_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

const vec3 UP(0.f, 1.f, 0.f);
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }

// The rainbow rank of each piece (I O T S Z J L): red Z, orange L, gold O, green S, teal I, blue J, violet T.
const int RANK[7] = {4, 2, 6, 3, 0, 5, 1};
// Each piece's spine (0 gilt rules and a label, 1 raised bands, 2 a label panel and gilt head, 3 rules and a fleuron).
const int STYLE[7] = {1, 2, 0, 3, 1, 2, 0};

// A light (sRGB): the seven bindings (I O T S Z J L), five old leathers for the shelves, the wood, the walls, the
// paper, the gilt, the floor, the rug, the leather, the lamp shades, the window's sky (top, bottom), the ambient
// light (sky, ground), the key light, the lamps, the candles; the fog below and above; the HUD's veil, ink and
// accent; a section's color; marble.
struct VariantDef {
    const char* name;
    unsigned piece[7], fill[5];
    unsigned wood, woodDark, plaster, paper, gilt, floor, rug, leather, shade, skyA, skyB, ambSky, ambGround, key, lamp, candle;
    unsigned fogLow, fogHigh, veil, ink, accent, section, marble;
    float night, moon, keyK, ambK, lampK, flicker, exposure;
    vec3 keyDir;
};
const VariantDef VARIANTS[] = {
    // warm lamplight at night: walnut, green damask, the window's night with its moon, brass and green glass
    {"LAMPLIGHT",
     {0x1f9aac, 0xe8b234, 0x8a4cc4, 0x2f9c58, 0xd23a2e, 0x3263c8, 0xea7f2a},
     {0x6a2420, 0x24432e, 0x22304e, 0x8c6a3c, 0x4a3122},
     0x6a4428, 0x3a2416, 0x2e4a3e, 0xefe4c8, 0xd8a84a, 0x7a4e2c, 0x7a1e22, 0x6e2a1c, 0x2f8a4e, 0x14244c, 0x2e4676, 0x3a4458, 0x2a1e16,
     0xffd8a8, 0xffb866, 0xffa040, 0x0a0806, 0x4a3420, 0x0c0a08, 0xf2e8d4, 0xe8b860, 0x2a1c12, 0xd8d0c4,
     1.f, 1.f, 0.7f, 0.55f, 1.2f, 0.15f, 1.05f, vec3(-0.35f, 0.75f, 0.55f)},
    // a daylight reading room: pale oak, cream plaster, a blue sky, the sun falling in from high on the left
    {"READING ROOM",
     {0x24a8c4, 0xf2c13e, 0x9a5ad2, 0x38b06a, 0xe2483a, 0x386ee2, 0xf28a30},
     {0x8a3a2c, 0x3c6a4c, 0x34507a, 0xc8a46a, 0xe4d6b8},
     0xa0764a, 0x6a4a2c, 0xe6dcc6, 0xf6eedc, 0xe2ba5c, 0xa87a4c, 0x2c4a7a, 0x7a3a24, 0x3a9a5e, 0x5e9ae6, 0xcfe4f8, 0xb8c4d8, 0x8a7458,
     0xfff0d8, 0xffd8a0, 0xffb860, 0x7a8294, 0xf2ead6, 0x14110c, 0xfaf4e6, 0xf0c870, 0x5a4030, 0xf0ebe2,
     0.f, 0.f, 2.4f, 1.0f, 0.55f, 0.f, 0.95f, vec3(-0.45f, 0.78f, 0.45f)},
    // a candlelit archive: dark oak, stone, candles guttering, the moon cold through the window
    {"CANDLELIT ARCHIVE",
     {0x22949e, 0xdca232, 0x8a4aa4, 0x3a8e4c, 0xc23428, 0x2e56aa, 0xd8742a},
     {0x4a1c16, 0x2e2c1e, 0x1c2230, 0x6a4e2c, 0xb8a47a},
     0x3e2a1a, 0x20140c, 0x50463a, 0xe2d4b0, 0xcc983e, 0x4e3a26, 0x4a1a18, 0x4a2016, 0x8a6a34, 0x0c1426, 0x1c2a44, 0x1c2028, 0x140e0a,
     0x9ab0e0, 0xff9a40, 0xff8a30, 0x030202, 0x2e1c0e, 0x080604, 0xeadcc0, 0xe4a452, 0x1a120c, 0x9a9084,
     1.f, 0.7f, 0.45f, 0.5f, 1.45f, 0.65f, 1.15f, vec3(-0.4f, 0.65f, 0.65f)},
    // blue hour: cool dusk in the window, silvery light, brass lamps just lit
    {"BLUE HOUR",
     {0x2ab0c8, 0xeec048, 0x9a62da, 0x3aa86a, 0xe0503e, 0x3a70e0, 0xf0903a},
     {0x5a2a34, 0x2a4a4a, 0x2a3a5e, 0x9a8a6a, 0x3a3040},
     0x5a3e30, 0x2e2018, 0x3a4a62, 0xece4d2, 0xd8b060, 0x6a4a36, 0x3a2a5a, 0x5a2a2a, 0x3a8a8a, 0x3a5aa0, 0xa08ab8, 0x5a6a90, 0x2a2430,
     0xc8d8ff, 0xffc070, 0xffa850, 0x080a14, 0x6a5a7a, 0x0a0a10, 0xf0ecf4, 0xf0c070, 0x241e2a, 0xc8c8d0,
     0.6f, 0.4f, 1.1f, 0.8f, 1.0f, 0.1f, 1.05f, vec3(0.4f, 0.7f, 0.6f)},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float SLIDE_T = 0.34f;     // a cleared book slides out of the well in this long, then flies
constexpr float CELL_H = 0.47f;      // a book in the well: its half width, half height, half depth
constexpr float CELL_HY = 0.48f;
constexpr float CELL_D = 0.36f;
constexpr float BOARD_EYE_Y = 4.5f;  // the board camera's height (the books' tops show)

// Test hooks: ZEN_LIBRARY_LINES=N adds N lines to the growth, ZEN_GROW_RATE=R adds R lines a second.
int extraLines(double t) {
    static const int n = std::getenv("ZEN_LIBRARY_LINES") ? std::atoi(std::getenv("ZEN_LIBRARY_LINES")) : 0;
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

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
    if (std::fabs(det) < 1e-20f) return mat4::identity();
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

// The world camera along the growth: where it looks (T), how far back (D), its yaw and pitch (degrees).
struct CamKey { float g, tx, ty, tz, d, yaw, pitch; };
const CamKey CAMS[] = {
    {0.f, 0.1f, 1.8f, -3.f, 6.2f, 0.f, -3.f},
    {11.f, 0.2f, 1.85f, -3.f, 6.6f, 0.f, -3.f},
    {17.f, 1.7f, 1.95f, -3.f, 8.4f, 0.f, -3.f},
    {60.f, 2.3f, 2.0f, -3.f, 9.4f, 0.f, -3.f},
    {105.f, 1.2f, 2.05f, -3.f, 10.4f, 0.f, -3.f},
    {130.f, 0.f, 2.1f, -3.f, 11.6f, 0.f, -3.f},
    {215.f, 0.f, 2.2f, -2.5f, 12.4f, 0.f, -3.f},
    {300.f, 0.f, 2.3f, -1.5f, 14.f, 0.f, -4.f},
};
constexpr int CAM_COUNT = (int)(sizeof(CAMS) / sizeof(CAMS[0]));

void camAt(float g, vec3& T, float& D, float& yaw, float& pitch) {
    int i = 0;
    while (i + 1 < CAM_COUNT - 1 && g > CAMS[i + 1].g) i++;
    const CamKey& a = CAMS[i];
    const CamKey& b = CAMS[std::min(i + 1, CAM_COUNT - 1)];
    float t = b.g > a.g ? saturate((g - a.g) / (b.g - a.g)) : 1.f;
    t = t * t * (3.f - 2.f * t);
    T = lerp(vec3(a.tx, a.ty, a.tz), vec3(b.tx, b.ty, b.tz), t);
    D = std::exp(lerpf(std::log(a.d), std::log(b.d), t));
    yaw = lerpf(a.yaw, b.yaw, t) * (PI / 180.f);
    pitch = lerpf(a.pitch, b.pitch, t) * (PI / 180.f);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

void LibraryUniverse::setupInstVao(GLuint vao, GLuint vbo, int mesh) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, meshVbo_[mesh]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshIbo_[mesh]);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < 6; i++) {
        glEnableVertexAttribArray(2 + i);
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Inst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(2 + i, 1);
    }
    glBindVertexArray(0);
}

bool LibraryUniverse::init(int width, int height) {
    using shaders::withCommon;
    progSolid_ = ugl::program(withCommon(shaders::SOLID_VS).c_str(), withCommon(shaders::SOLID_FS, true).c_str(), "library solids");
    progShadow_ = ugl::program(withCommon(shaders::SOLID_VS).c_str(), shaders::SHADOW_FS, "library shadow");
    progSprite_ = ugl::program(withCommon(shaders::SPRITE_VS).c_str(), withCommon(shaders::SPRITE_FS).c_str(), "library glows");
    progLine_ = ugl::program(shaders::LINE_VS, shaders::LINE_FS, "library lines");
    progVeil_ = ugl::program(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "library veil");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "library down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "library blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "library final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "library shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "library text");

    // ---- The meshes: a box, a cylinder, a hemisphere (position + normal, indexed).
    {
        std::vector<float> v[MESH_COUNT];
        std::vector<unsigned> idx[MESH_COUNT];
        auto vert = [&](int m, const vec3& p, const vec3& n) {
            v[m].insert(v[m].end(), {p.x, p.y, p.z, n.x, n.y, n.z});
            return (unsigned)(v[m].size() / 6 - 1);
        };
        // box: 6 faces
        for (int f = 0; f < 6; f++) {
            const int ax = f / 2;
            const float sg = (f % 2) ? -1.f : 1.f;
            const int a1 = (ax + 1) % 3, a2 = (ax + 2) % 3;
            unsigned base = 0;
            const float cs[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
            for (int k = 0; k < 4; k++) {
                float p[3], n[3] = {0, 0, 0};
                p[ax] = sg;
                p[a1] = cs[k][0];
                p[a2] = cs[k][1] * sg;
                n[ax] = sg;
                const unsigned id = vert(0, vec3(p[0], p[1], p[2]), vec3(n[0], n[1], n[2]));
                if (k == 0) base = id;
            }
            idx[0].insert(idx[0].end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
        // cylinder: 20 sides, caps
        {
            const int S = 20;
            for (int i = 0; i <= S; i++) {
                const float a = TAU * i / S;
                const vec3 n(std::cos(a), 0.f, std::sin(a));
                vert(1, vec3(n.x, -1.f, n.z), n);
                vert(1, vec3(n.x, 1.f, n.z), n);
            }
            for (int i = 0; i < S; i++) {
                const unsigned a = i * 2, b = a + 1, c = a + 2, d = a + 3;
                idx[1].insert(idx[1].end(), {a, b, d, a, d, c});
            }
            for (int s = -1; s <= 1; s += 2) {
                const unsigned c = vert(1, vec3(0.f, (float)s, 0.f), vec3(0.f, (float)s, 0.f));
                const unsigned first = (unsigned)(v[1].size() / 6);
                for (int i = 0; i <= S; i++) {
                    const float a = TAU * i / S;
                    vert(1, vec3(std::cos(a), (float)s, std::sin(a)), vec3(0.f, (float)s, 0.f));
                }
                for (int i = 0; i < S; i++) {
                    if (s > 0) idx[1].insert(idx[1].end(), {c, first + i + 1, first + i});
                    else idx[1].insert(idx[1].end(), {c, first + i, first + i + 1});
                }
            }
        }
        // hemisphere (y >= 0)
        {
            const int LA = 16, LO = 48;
            for (int j = 0; j <= LA; j++) {
                const float el = (PI * 0.5f) * j / LA;
                for (int i = 0; i <= LO; i++) {
                    const float az = TAU * i / LO;
                    const vec3 n(std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az));
                    vert(2, n, n);
                }
            }
            for (int j = 0; j < LA; j++)
                for (int i = 0; i < LO; i++) {
                    const unsigned a = j * (LO + 1) + i, b = a + 1, c = a + (LO + 1), d = c + 1;
                    idx[2].insert(idx[2].end(), {a, c, b, b, c, d});
                }
        }
        for (int m = 0; m < MESH_COUNT; m++) {
            glGenBuffers(1, &meshVbo_[m]);
            glBindBuffer(GL_ARRAY_BUFFER, meshVbo_[m]);
            glBufferData(GL_ARRAY_BUFFER, v[m].size() * sizeof(float), v[m].data(), GL_STATIC_DRAW);
            glGenBuffers(1, &meshIbo_[m]);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshIbo_[m]);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx[m].size() * sizeof(unsigned), idx[m].data(), GL_STATIC_DRAW);
            meshN_[m] = (GLsizei)idx[m].size();
            glGenVertexArrays(1, &statVao_[m]);
            glGenBuffers(1, &statVbo_[m]);
            setupInstVao(statVao_[m], statVbo_[m], m);
            glGenVertexArrays(1, &dynVao_[m]);
            glGenBuffers(1, &dynVbo_[m]);
            setupInstVao(dynVao_[m], dynVbo_[m], m);
        }
    }
    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
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
    }
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

    // ---- The sort units' times (a float texture), the key light's shadow map.
    glGenTextures(1, &sortTex_);
    glBindTexture(GL_TEXTURE_2D, sortTex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 2048, 2048, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
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

    buildFont(caps_, FONT_CAPS);
    buildFont(ital_, FONT_ITAL);
    resize(width, height);
    // dust motes: scattered in a unit box, placed around the camera's target as it moves
    motes_.clear();
    Rng r(0x5EED);
    for (int i = 0; i < 360; i++) motes_.push_back({vec3(r.uniform(), r.uniform(), r.uniform()), r.uniform()});
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

void LibraryUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] library font init failed\n");
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
        const int cp = ch == 127 ? 0xB7 : ch;
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

// The board: the reference camera raised to BOARD_EYE_Y and looking straight ahead, its image shifted back down so
// the board plane keeps the reference pixels.
void LibraryUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    aspect_ = (float)width_ / height_;
    const layout::Camera ref = layout::camera(width_, height_);
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    const float focal = 1.f / std::tan(half);
    eyeB_ = vec3(ref.target.x, BOARD_EYE_Y, ref.target.z + ref.dist);
    const mat4 view = mat4::lookAt(eyeB_, eyeB_ + vec3(0.f, 0.f, -1.f), UP);
    mat4 shift = mat4::identity();
    shift.at(1, 3) = focal * (BOARD_EYE_Y - ref.target.y) / ref.dist;
    vpB_ = shift * mat4::perspective(2.f * half, aspect_, 0.5f, 400.f) * view;
    const float hh = ref.dist * std::tan(half);
    viewH_ = vec2{hh * aspect_, hh};
    viewC_ = vec2{ref.target.x, ref.target.y};
    fB_ = height_ * 0.5f * focal;

    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false);
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
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA16F, width_, height_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

LibraryUniverse::~LibraryUniverse() {
    for (ugl::Target* t : {&sceneT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progSolid_, progShadow_, progSprite_, progLine_, progVeil_, progDown_, progBlur_, progFinal_, progShape_, progText_})
        glDeleteProgram(p);
    for (int m = 0; m < MESH_COUNT; m++) {
        glDeleteVertexArrays(1, &statVao_[m]);
        glDeleteVertexArrays(1, &dynVao_[m]);
        glDeleteBuffers(1, &statVbo_[m]);
        glDeleteBuffers(1, &dynVbo_[m]);
        glDeleteBuffers(1, &meshVbo_[m]);
        glDeleteBuffers(1, &meshIbo_[m]);
    }
    for (GLuint v : {spriteVao_, lineVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, lineVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    if (shadowTex_) glDeleteTextures(1, &shadowTex_);
    if (sortTex_) glDeleteTextures(1, &sortTex_);
    for (Font* f : {&caps_, &ital_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

vec3 LibraryUniverse::pieceColor(int type) const { return pal_.piece[std::clamp(type, 0, 6)]; }
int LibraryUniverse::pieceStyle(int type) const { return STYLE[std::clamp(type, 0, 6)]; }

void LibraryUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 2;              // the candlelit archive
        else if (t.hasSong && !t.minor && t.brightness > 0.58f) v = 1;        // the reading room by day
        else if (t.hasSong && t.airWeight > 0.6f) v = 3;                      // blue hour
        else if (t.hasSong) v = 0;                                            // lamplight
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    const bool changed = v != variant_ || !colSet_;
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    Palette& P = palT_;
    for (int i = 0; i < 7; i++) P.piece[i] = lin(V.piece[i]);
    for (int i = 0; i < 7; i++) P.rain[RANK[i]] = lin(V.piece[i]);
    for (int i = 0; i < 5; i++) P.fill[i] = lin(V.fill[i]);
    P.wood = lin(V.wood); P.woodDark = lin(V.woodDark); P.plaster = lin(V.plaster); P.paper = lin(V.paper); P.gilt = lin(V.gilt);
    P.floor = lin(V.floor); P.rug = lin(V.rug); P.leather = lin(V.leather); P.shade = lin(V.shade); P.skyA = lin(V.skyA); P.skyB = lin(V.skyB);
    P.ambSky = lin(V.ambSky) * V.ambK; P.ambGround = lin(V.ambGround) * V.ambK; P.key = lin(V.key) * V.keyK; P.lampCol = lin(V.lamp);
    P.candle = lin(V.candle); P.fogLow = lin(V.fogLow); P.fogHigh = lin(V.fogHigh); P.veil = lin(V.veil); P.ink = hex(V.ink);
    P.accent = hex(V.accent); P.section = lin(V.section); P.marble = lin(V.marble);
    P.night = V.night; P.moon = V.moon; P.keyK = V.keyK; P.lampK = V.lampK; P.flicker = V.flicker; P.exposure = V.exposure;
    const bool instant = !colSet_ || seconds <= 0.1f;
    // the world's colors are baked in its parts: a new variant rebuilds it (and switches at once)
    pal_ = palT_;
    colSet_ = true;
    (void)instant;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x11B7ull);
    if (changed) worldDirty_ = true;
}

void LibraryUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

void LibraryUniverse::drop(float strength) {
    (void)strength;
    dropMode_ = true;
}

vec2 LibraryUniverse::project(const vec3& p) const { return layout::project(vpB_, p, width_, height_); }

TransitionHints LibraryUniverse::hints() const {
    TransitionHints h;
    h.background = lerp(pal_.plaster, pal_.wood, 0.5f) * (pal_.night > 0.5f ? 0.5f : 0.8f);
    h.light = false;
    h.ink = lerp(pal_.paper, pal_.gilt, 0.3f) * 0.8f;
    h.accent = pal_.gilt;
    h.styles = TS_REBUILD | TS_LUMA | TS_INK;
    return h;
}

// =====================================================================================================
// The collection
// =====================================================================================================

// A book's center on a shelf, at x along it.
vec3 LibraryUniverse::bookPos(const Book& b, int shelf, float x) const {
    if (shelf < 0 || shelf >= (int)shelves_.size()) return vec3(0.f, -100.f, 0.f);
    const Shelf& s = shelves_[shelf];
    return s.o + s.along * x + UP * (b.h * 0.5f) - s.out * (b.d * 0.5f);
}

// Packs the books in their order along the shelves (each its own thickness, a hair between), adding bookcases when
// the shelves run out.
void LibraryUniverse::layoutBooks() {
    int shelf = 0;
    float x = 0.f;
    for (Book& b : books_) {
        for (;;) {
            if (shelf >= (int)shelves_.size()) {
                if ((int)bays_.size() >= 30) { b.shelf = -1; break; }
                newBay();
                worldDirty_ = true;
                // the new bay's shelves (worldDirty rebuilds everything; record them now for the layout)
                const Bay& nb = bays_.back();
                const float y0 = 0.12f, y1 = TIER_H - 0.12f;
                const float pitch = (y1 - y0) / BAY_SHELVES;
                const float innerW = BAY_W - 0.1f;
                for (int s = BAY_SHELVES - 1; s >= 0; s--) {
                    Shelf sh;
                    const float yb = y0 + s * pitch;
                    sh.o = nb.c - nb.along * (innerW * 0.5f) + UP * (yb + 0.025f) - nb.out * 0.035f;
                    sh.along = nb.along;
                    sh.out = nb.out;
                    sh.len = innerW;
                    sh.clear = pitch - 0.03f;
                    sh.bay = (int)bays_.size() - 1;
                    shelves_.push_back(sh);
                }
            }
            const Shelf& s = shelves_[shelf];
            if (x + b.w <= s.len || x == 0.f) {
                if (b.shelf != shelf && b.shelf >= 0 && !b.flying) {
                    b.hopFrom = b.shelfShown;
                    b.hopX = b.x;
                    b.hop = 0.f;
                }
                b.shelf = shelf;
                b.xT = x + b.w * 0.5f;
                if (b.shelfShown < 0 || b.flying) { b.shelfShown = shelf; b.x = b.xT; }
                x += b.w + 0.003f;
                break;
            }
            shelf++;
            x = 0.f;
        }
    }
}

void LibraryUniverse::resetGrowth() {
    books_.clear();
    flights_.erase(std::remove_if(flights_.begin(), flights_.end(), [](const Flight& f) { return f.kind == 0; }), flights_.end());
    bays_.clear();
    hookLines_ = 0;
    worldDirty_ = true;
}

// A new book for the collection, in its sorted place (the books after it make room).
const LibraryUniverse::Book& LibraryUniverse::addBook(int type, bool flying) {
    Book b;
    b.id = nextBookId_++;
    b.type = type;
    b.style = pieceStyle(type);
    b.h = rng_.range(0.24f, 0.335f);
    b.w = rng_.range(0.032f, 0.056f);
    b.d = std::min(0.3f, rng_.range(0.15f, 0.21f) + (b.h - 0.23f) * 0.4f);
    b.gilt = rng_.chance(0.3f);
    b.key = RANK[type] * 10.f + (0.4f - b.h) * 10.f + rng_.uniform() * 0.01f;
    b.flying = flying;
    auto it = std::upper_bound(books_.begin(), books_.end(), b.key, [](float key, const Book& o) { return key < o.key; });
    return *books_.insert(it, b);
}

// =====================================================================================================
// Game events
// =====================================================================================================

void LibraryUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Clear: {
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        std::sort(rows.begin(), rows.end());
        if (ev.count >= 4) tetrisT_ = 0.f;
        for (auto& c : ev.cells) {
            if (c.x < 0 || c.x >= Game::W || c.y < Game::HIDDEN) continue;
            const int k = (int)(std::find(rows.begin(), rows.end(), c.y) - rows.begin());
            const int type = std::clamp(c.type, 0, 6);
            // a book for the collection, put in its sorted place (the books after it make room at once)
            const Book& b = addBook(type, true);
            Flight fl;
            fl.book = b.id;
            fl.type = type;
            fl.style = b.style;
            fl.gilt = b.gilt;
            fl.b0 = layout::cellPos((float)c.x, (float)c.y);
            fl.delay = 0.035f * (float)((c.x * 7) % 10) * 0.5f + 0.1f * k;
            fl.spin = 0.f;
            fl.kind = 0;
            flights_.push_back(fl);
        }
        layoutBooks();
        break;
    }
    case GameEvent::TopOut: {
        // the stack's books tumble out of the well
        overT_ = 0.f;
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            Flight fl;
            fl.kind = 1;
            fl.type = std::clamp(c.type, 0, 6);
            fl.style = pieceStyle(fl.type);
            fl.b0 = layout::cellPos((float)c.x, (float)c.y);
            fl.delay = 0.07f * (c.y - top) + 0.2f * hashf((uint64_t)(c.x * 31 + c.y));
            fl.vel = vec3(rng_.range(-1.5f, 1.5f), rng_.range(0.5f, 3.f), rng_.range(1.f, 4.f));
            fl.rotV = rng_.range(-4.f, 4.f);
            flights_.push_back(fl);
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Pages, flights
// =====================================================================================================

// The hand-off: the book leaves the well's space for the library's, at the same pixel and the same apparent size.
void LibraryUniverse::launch(Flight& fl) {
    const vec3 bp = fl.b0 + vec3(0.f, 0.25f, 1.25f - CELL_D);
    // its pixel and its height in pixels through the board's camera
    const vec4 c0 = vpB_ * vec4(bp, 1.f);
    const vec4 ct = vpB_ * vec4(bp + vec3(0.f, CELL_HY, 0.f), 1.f);
    const vec2 ndc{c0.x / c0.w, c0.y / c0.w};
    const float hpx = std::fabs(ct.y / ct.w - c0.y / c0.w) * height_ * 0.5f * 2.f;
    // the world ray through that pixel
    const vec3 dir = normalize(fwdW_ + rightW_ * (ndc.x * tanW_ * aspect_) + upW_ * (ndc.y * tanW_));
    const Book* bk = nullptr;
    for (const Book& b : books_)
        if (b.id == fl.book) bk = &b;
    vec3 target = targetW_;
    if (bk && bk->shelf >= 0) target = bookPos(*bk, bk->shelf, bk->xT);
    const float dist = std::max(0.5f, dot(target - eyeW_, fwdW_));
    const float d0 = std::clamp(dist * 0.32f, 0.35f, 60.f);
    const float H0 = hpx * d0 * tanW_ / (height_ * 0.5f);
    const float k = H0 / (2.f * CELL_HY);
    fl.dims0 = vec3(CELL_H, CELL_HY, CELL_D) * k;
    fl.p0 = eyeW_ + dir * (d0 / std::max(0.2f, dot(dir, fwdW_)));
    fl.face0 = normalize(vec3(-fwdW_.x, 0.f, -fwdW_.z) + vec3(0.f, -fwdW_.y * 0.5f, 0.f));
    fl.world = true;
    fl.t = 0.f;
    const float L = length(target - fl.p0);
    fl.dur = 1.9f + 0.5f * std::log2(1.f + L / 3.f);
    fl.p1 = fl.p0 + UP * (0.1f * L) + fwdW_ * (0.15f * L);
    if (bk) fl.dims1 = vec3(bk->w * 0.5f, bk->h * 0.5f, bk->d * 0.5f);
    else fl.dims1 = fl.dims0 * 0.2f;
}

// =====================================================================================================
// The board
// =====================================================================================================

void LibraryUniverse::buildBoard(const Game& g) {
    auto book = [&](const vec3& c, float sc, int type, float seed, float glow, float tilt = 0.f) {
        Inst in;
        const vec3 col = pieceColor(type);
        const float ct = std::cos(tilt), stl = std::sin(tilt);
        const vec3 X(CELL_H * sc, 0, 0), Y(0, CELL_HY * sc * ct, CELL_HY * sc * stl), Z(0, -CELL_D * sc * stl, CELL_D * sc * ct);
        const vec3 cc(c.x, c.y, c.z - CELL_D * sc);
        in.c[0] = cc.x; in.c[1] = cc.y; in.c[2] = cc.z; in.c[3] = (float)M_BOOK;
        in.x[0] = X.x; in.x[1] = X.y; in.x[2] = X.z; in.x[3] = (float)pieceStyle(type);
        in.y[0] = Y.x; in.y[1] = Y.y; in.y[2] = Y.z; in.y[3] = glow;
        in.z[0] = Z.x; in.z[1] = Z.y; in.z[2] = Z.z; in.z[3] = (type == 1 || type == 5) ? 1.f : 0.f;
        in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = seed;
        in.e[0] = -100.f; in.e[1] = -1.f; in.e[2] = -1.f; in.e[3] = 0.f;
        cells_.push_back(in);
    };
    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            book(p, 1.f, c.type, 0.13f * x + 0.71f * y, 0.f);
        }
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) book(layout::cellPos(cells[i][0] + dx, cells[i][1] + dy), 1.f, p.type, 0.37f * i + 0.11f * p.type, 0.05f);
        // the ghost: a gilt outline where it lands
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
            const vec3 col = lerp(pieceColor(p.type), pal_.gilt, 0.5f) * 1.4f;
            const float wpx = std::max(1.f, 1.5f * height_ / 1080.f);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                const float h = 0.46f, z = 0.01f;
                auto L = [&](const vec3& a, const vec3& b) {
                    cellLines_.push_back({{a.x, a.y, a.z, wpx}, {b.x, b.y, b.z, 0.7f}, {col.x, col.y, col.z, 0.f}});
                };
                if (!has(c[0], c[1] - 1)) L(vec3(w.x - h, w.y + h, z), vec3(w.x + h, w.y + h, z));
                if (!has(c[0], c[1] + 1)) L(vec3(w.x - h, w.y - h, z), vec3(w.x + h, w.y - h, z));
                if (!has(c[0] - 1, c[1])) L(vec3(w.x - h, w.y - h, z), vec3(w.x - h, w.y + h, z));
                if (!has(c[0] + 1, c[1])) L(vec3(w.x + h, w.y - h, z), vec3(w.x + h, w.y + h, z));
            }
        }
    }
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
            book(vec3(center.x + ox * sc, center.y - oy * sc, 0.f), sc, type, 0.21f * i, 0.f);
        }
    };
    if (!g.over()) {
        preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0));
        for (int i = 0; i < 3; i++) {
            const vec3 c = layout::nextCenter(i);
            preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i));
        }
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float LibraryUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) w += f.g[glyphIndex((unsigned char)s[i])].adv * k + track;
    return w - track;
}

void LibraryUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const {
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

void LibraryUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha, float haloA) {
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

void LibraryUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

void LibraryUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    // cream lettering, gilt for the accents, on a soft dark halo
    const vec3 ink = pal_.ink, halo = ugl::toSrgb(pal_.veil) * 0.6f;
    const vec3 accent = pal_.accent;
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                    {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }
    // the bonus gauge: a fine ring ticked like a clock's dial, its charge in gilt
    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 120;
        const float th = std::max(1.2f, 1.4f * s), th2 = std::max(2.5f, 4.f * s);
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
        for (int i = 0; i < N; i++) seg(TAU * i / N, TAU * (i + 1) / N, r, th, ink, (g.bonusReady() ? 0.55f : 0.25f) * dimk);
        for (int i = 0; i < 12; i++) {
            const float a = TAU * i / 12;
            seg(a - 0.01f, a + 0.01f, r - th2 * 1.1f, th2 * (i % 3 == 0 ? 1.2f : 0.7f), ink, 0.4f * dimk);
        }
        flushShapes();
        for (int i = 0; i < N; i++) {
            if ((i + 0.5f) / N > fill) break;
            seg(TAU * i / N, TAU * (i + 1) / N, r, th2, accent, 0.92f * dimk);
        }
        flushShapes();
    }
    std::vector<float> q;
    auto upper = [](std::string t) {
        for (char& c : t) c = (char)std::toupper((unsigned char)c);
        return t;
    };
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha) * vis;
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(ital_, h.text, base * 0.78f, base * 0.04f, base * 0.04f);
            drawText(ital_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f) * 0.8f * vis);
            continue;
        case HUD_SECTION:
            place(caps_, upper(h.text), base * 0.72f, base * 0.28f, -base * 0.02f);
            drawText(caps_, q, accent, halo, saturate(0.3f + h.alpha * 1.7f) * vis);
            continue;
        case HUD_TITLE:
            place(ital_, h.text, base * 1.05f, base * 0.01f, -base * 0.1f);
            drawText(ital_, q, ink, halo, saturate(h.alpha * 1.3f) * vis);
            continue;
        case HUD_INFO:
            place(caps_, upper(h.text), base * 0.58f, base * 0.22f);
            drawText(caps_, q, ink, halo, saturate(alpha * 1.2f) * 0.8f);
            continue;
        case HUD_LABEL:
            place(caps_, upper(h.text), base * 0.62f, base * 0.3f);
            drawText(caps_, q, accent, halo, saturate(alpha * 1.3f) * 0.9f);
            continue;
        case HUD_VALUE:
            place(caps_, h.text, base * 0.92f, base * 0.06f, -base * 0.04f);
            drawText(caps_, q, ink, halo, alpha * 0.96f);
            continue;
        case HUD_NOTICE:
            place(ital_, h.text, base * 0.95f, base * 0.02f);
            drawText(ital_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(caps_, h.text, base * 0.72f, base * 0.1f);
            drawText(caps_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(caps_, upper(h.text), base * 0.86f, base * 0.3f, -base * 0.1f);
            drawText(caps_, q, ink, halo, alpha, 0.65f);
            continue;
        case HUD_TOAST_SUB:
            place(ital_, h.text, base * 0.95f, base * 0.04f);
            drawText(ital_, q, accent, halo, alpha * 0.9f, 0.65f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(caps_, upper(h.text), base * 0.8f, base * 0.26f);
                drawText(caps_, q, ink, halo, alpha, 0.75f);
            } else {
                place(ital_, h.text, base * 0.95f, base * 0.02f);
                drawText(ital_, q, h.accent ? accent : ink, halo, alpha, 0.75f);
            }
            continue;
        default:
            place(caps_, h.text, base * 0.7f, base * 0.06f);
            drawText(caps_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// The camera
// =====================================================================================================

void LibraryUniverse::updateCamera(float dt) {
    // the camera follows the growth a moment behind it (a spring)
    if (camG_ < 0.f) { camG_ = grow_; camGv_ = 0.f; }
    const float w = 1.1f;
    camGv_ += (w * w * (grow_ - camG_) - 2.f * w * camGv_) * dt;
    camG_ += camGv_ * dt;
    vec3 T;
    float D, yaw, pitch;
    camAt(camG_, T, D, yaw, pitch);
    // a slow drift, never still
    yaw += 0.02f * std::sin(worldTime_ * 0.05f) * smoothstepf(4.f, 20.f, camG_) + 0.012f * std::sin(worldTime_ * 0.031f);
    pitch += 0.008f * std::sin(worldTime_ * 0.043f);
    fwdW_ = vec3(-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch));
    rightW_ = normalize(cross(fwdW_, UP));
    upW_ = cross(rightW_, fwdW_);
    targetW_ = T;
    camD_ = D;
    eyeW_ = T - fwdW_ * D;
    tanW_ = std::tan(25.f * PI / 180.f);
    const float zn = std::max(0.04f, D * 0.012f), zf = std::max(80.f, D * 12.f);
    const mat4 view = mat4::lookAt(vec3(0.f), fwdW_, UP);
    vpW_ = mat4::perspective(2.f * std::atan(tanW_), aspect_, zn, zf) * view;
    invVpW_ = inverse(vpW_);
    cut_ = vec4(0.f, 0.f, 0.f, 1e9f);
}

void LibraryUniverse::drawInst(GLuint vao, GLuint vbo, int mesh, const std::vector<Inst>* data, size_t count) {
    if (!count) return;
    glBindVertexArray(vao);
    if (data) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(Inst)), data->data(), GL_STREAM_DRAW);
    }
    glUniform1i(ugl::U(progSolid_, "uMesh"), mesh);
    glDrawElementsInstanced(GL_TRIANGLES, meshN_[mesh], GL_UNSIGNED_INT, nullptr, (GLsizei)count);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void LibraryUniverse::render(const Frame& f, GLuint fbo) {
    // ZEN_LIBRARY_TIMING=1: the frame's cost (GPU included: glFinish around it), every 200 frames
    static const bool timing = std::getenv("ZEN_LIBRARY_TIMING") != nullptr;
    struct Timer {
        bool on;
        double t0 = 0;
        const LibraryUniverse* u;
        Timer(bool o, const LibraryUniverse* uu) : on(o), u(uu) {
            if (on) { glFinish(); t0 = (double)clock_gettime_ms(); }
        }
        static double clock_gettime_ms() {
            timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
        }
        ~Timer() {
            if (!on) return;
            glFinish();
            static double acc = 0, mx = 0;
            static int n = 0;
            const double d = clock_gettime_ms() - t0;
            acc += d;
            mx = std::max(mx, d);
            if (++n == 200) {
                std::fprintf(stderr, "[library] frame %.2f ms (max %.2f) at %.0f lines\n", acc / n, mx, u->grow_);
                acc = 0; mx = 0; n = 0;
            }
        }
    } timer(timing, this);
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.0f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
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
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = dropMode_ && overT_ < 0.f && (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f);
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.2f : 0.5f, dt);
    waveT_ += dt;
    tetrisT_ += dt;
    kickK_ = std::max(kickK_ * std::exp(-dt * 5.f), kickIn_);
    kickIn_ = 0.f;
    refreshK_ = approach(refreshK_, 0.f, 0.5f, dt);
    beatPulse_ = music.hasSong ? std::pow(1.f - (float)(music.beat - std::floor(music.beat)), 3.f) : 0.f;
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- The game's lines -> the growth (a new game starts again from the study).
    {
        const int hook = extraLines(time_);
        const int lines = game.lines() + hook;
        if (linesSeen_ >= 0 && lines < lines_ && !game.over()) resetGrowth();
        lines_ = lines;
        if (linesSeen_ < 0) { grow_ = (float)lines; growV_ = 0.f; }
        linesSeen_ = lines;
        // (the test hooks' lines bring their ten books each, put straight on the shelves)
        if (hook > hookLines_) {
            for (int l = hookLines_; l < hook; l++)
                for (int k = 0; k < 10; k++) addBook(rng_.irange(0, 6), false);
            hookLines_ = hook;
            layoutBooks();
            for (Book& b : books_)
                if (!b.flying) { b.shelfShown = b.shelf; b.x = b.xT; b.hop = 1.f; }
        }
        const float target = (float)lines;
        const float w = 3.2f;
        growV_ += (w * w * (target - grow_) - 2.f * w * growV_) * wdt;
        grow_ += growV_ * wdt;
        if (std::fabs(grow_ - target) < 0.002f && std::fabs(growV_) < 0.01f) grow_ = target;
    }
    // collection bays that take over a filler bookcase: the filler goes when its bay comes
    if (worldDirty_) {
        buildWorld();
        // stat buffers: nearest first so the depth test rejects most hidden fragments
        for (int m = 0; m < MESH_COUNT; m++) {
            glBindBuffer(GL_ARRAY_BUFFER, statVbo_[m]);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(stat_[m].size() * sizeof(Inst)), stat_[m].empty() ? nullptr : stat_[m].data(), GL_STATIC_DRAW);
            statCount_[m] = stat_[m].size();
        }
        layoutBooks();
        static bool once = false;
        if (!once) {
            once = true;
            std::fprintf(stderr, "[library] world: %zu boxes, %zu cylinders, %zu domes, %zu lamps\n", stat_[0].size(),
                         stat_[1].size(), stat_[2].size(), lamps_.size());
        }
    }
    if (sortDirty_) {
        const int W = 1024, H = std::max(1, ((int)sortTimes_.size() + W - 1) / W);
        std::vector<float> tex((size_t)W * H, -1.f);
        std::copy(sortTimes_.begin(), sortTimes_.end(), tex.begin());
        glBindTexture(GL_TEXTURE_2D, sortTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, W, H, 0, GL_RED, GL_FLOAT, tex.data());
        sortDirty_ = false;
    }
    updateCamera(wdt > 0.f ? wdt : dt * 0.f);

    // ---- The collection's books: sliding to their places, hopping to another shelf.
    for (Book& b : books_) {
        b.glow = approach(b.glow, 0.f, 1.5f, dt);
        if (b.flying) continue;
        if (b.hop < 1.f) {
            b.hop = std::min(1.f, b.hop + wdt / 0.7f);
            if (b.hop >= 1.f) { b.shelfShown = b.shelf; b.x = b.xT; }
        } else {
            b.shelfShown = b.shelf;
            b.x = approach(b.x, b.xT, 7.f, wdt);
        }
    }

    // ---- Flights.
    for (Flight& fl : flights_) {
        fl.t += (fl.world || fl.kind == 1) ? wdt : dt;
        if (fl.kind == 1) continue;
        if (!fl.world && fl.t - fl.delay >= SLIDE_T) launch(fl);
    }

    // ---- Dynamic instances: the collection's bookcases and books, the pages, the flights.
    for (auto& v : dyn_) v.clear();
    flightInst_.clear();
    cells_.clear();
    cellLines_.clear();
    sprites_.clear();
    putTo_ = dyn_;
    for (int i = 0; i < (int)bays_.size(); i++) {
        const Bay& b = bays_[i];
        st_ = Stamp{};
        st_.born = b.born;
        st_.style = i == 0 ? 0.f : 3.f;
        buildBookcase(b.c, b.along, b.out, BAY_W, TIER_H, BAY_SHELVES, pal_.wood, false, -1, 3.f + i);
        // brass bookends at the shelves' left ends
    }
    st_ = Stamp{};
    auto putBook = [&](std::vector<Inst>& out, const vec3& c, const vec3& X, const vec3& Y, const vec3& Z, int type, int style, float glow, bool gilt,
                       float seed, float born) {
        Inst in;
        const vec3 col = pieceColor(type);
        in.c[0] = c.x; in.c[1] = c.y; in.c[2] = c.z; in.c[3] = (float)M_BOOK;
        in.x[0] = X.x; in.x[1] = X.y; in.x[2] = X.z; in.x[3] = (float)style;
        in.y[0] = Y.x; in.y[1] = Y.y; in.y[2] = Y.z; in.y[3] = glow;
        in.z[0] = Z.x; in.z[1] = Z.y; in.z[2] = Z.z; in.z[3] = gilt ? 1.f : 0.f;
        in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = seed;
        in.e[0] = born; in.e[1] = -1.f; in.e[2] = -1.f; in.e[3] = 0.f;
        out.push_back(in);
    };
    const int nBooks = (int)books_.size();
    int bi = -1;
    for (const Book& b : books_) {
        bi++;
        if (b.flying || b.shelfShown < 0) continue;
        // a soft light running along the spines: slowly round the collection for a drop, once along it for a Tetris
        float g = b.glow;
        if (nBooks > 0) {
            const float f = (bi + 0.5f) / nBooks;
            if (dropAmt_ > 0.01f) {
                float d = std::fabs(f - std::fmod(worldTime_ * 0.06f, 1.2f) + 0.1f);
                g += 0.45f * dropAmt_ * std::exp(-d * 18.f);
            }
            if (tetrisT_ < 5.f) g += 0.4f * std::exp(-std::fabs(f - tetrisT_ / 4.f) * 20.f);
        }
        const Shelf& s = shelves_[b.shelfShown];
        vec3 p;
        if (b.hop < 1.f && b.hopFrom >= 0) {
            const vec3 a = bookPos(b, b.hopFrom, b.hopX), c = bookPos(b, b.shelf, b.xT);
            const float e = smoothstepf(0.f, 1.f, b.hop);
            p = lerp(a, c, e) + s.out * (0.25f * std::sin(PI * b.hop)) + UP * (0.12f * std::sin(PI * b.hop));
        } else {
            p = bookPos(b, b.shelfShown, b.x);
        }
        putBook(dyn_[MESH_BOX], p, s.along * (b.w * 0.5f), UP * (b.h * 0.5f), s.out * (b.d * 0.5f), b.type, b.style, g, b.gilt,
                (float)(b.id % 97) * 0.13f, -100.f);
    }
    putTo_ = nullptr;
    // flights: in the well's space while sliding out, then in the library's
    for (auto it = flights_.begin(); it != flights_.end();) {
        Flight& fl = *it;
        const float tt = fl.t - fl.delay;
        if (fl.kind == 1) {
            // game over: tumbling out of the well
            const float t = std::max(0.f, tt);
            const vec3 p = fl.b0 + vec3(fl.vel.x * t, fl.vel.y * t - 9.f * t * t, fl.vel.z * t);
            const float a = fl.rotV * t;
            const vec3 col = pieceColor(fl.type);
            Inst in{};
            const vec3 X(CELL_H * std::cos(a), CELL_H * std::sin(a), 0.f), Y(-CELL_HY * std::sin(a), CELL_HY * std::cos(a), 0.f), Z(0, 0, CELL_D);
            in.c[0] = p.x; in.c[1] = p.y; in.c[2] = p.z - CELL_D; in.c[3] = (float)M_BOOK;
            in.x[0] = X.x; in.x[1] = X.y; in.x[2] = X.z; in.x[3] = (float)fl.style;
            in.y[0] = Y.x; in.y[1] = Y.y; in.y[2] = Y.z;
            in.z[0] = Z.x; in.z[1] = Z.y; in.z[2] = Z.z;
            in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = fl.b0.x;
            in.e[0] = -100.f; in.e[1] = -1.f; in.e[2] = -1.f;
            if (tt >= 0.f) cells_.push_back(in);
            if (tt > 3.f) { it = flights_.erase(it); continue; }
            ++it;
            continue;
        }
        if (!fl.world) {
            // sliding out of the well toward us, its head tipping forward a little
            const float s = saturate(tt / SLIDE_T);
            const float e = s * s * (3.f - 2.f * s);
            const vec3 p = fl.b0 + vec3(0.f, 0.25f * e, (1.25f) * e);
            Inst in{};
            const float tilt = -0.25f * std::sin(PI * s);
            const float ct = std::cos(tilt), stl = std::sin(tilt);
            const vec3 col = pieceColor(fl.type);
            const vec3 X(CELL_H, 0, 0), Y(0, CELL_HY * ct, CELL_HY * stl), Z(0, -CELL_D * stl, CELL_D * ct);
            in.c[0] = p.x; in.c[1] = p.y; in.c[2] = p.z - CELL_D; in.c[3] = (float)M_BOOK;
            in.x[0] = X.x; in.x[1] = X.y; in.x[2] = X.z; in.x[3] = (float)fl.style;
            in.y[0] = Y.x; in.y[1] = Y.y; in.y[2] = Y.z; in.y[3] = 0.5f * std::sin(PI * s);
            in.z[0] = Z.x; in.z[1] = Z.y; in.z[2] = Z.z; in.z[3] = fl.gilt ? 1.f : 0.f;
            in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = fl.b0.x * 0.3f;
            in.e[0] = -100.f; in.e[1] = -1.f; in.e[2] = -1.f;
            if (tt >= 0.f) cells_.push_back(in);
            ++it;
            continue;
        }
        // across the library: a curve up and over, then pushed home into its gap
        Book* bk = nullptr;
        for (Book& b : books_)
            if (b.id == fl.book) bk = &b;
        vec3 p3 = targetW_, out = vec3(0, 0, 1);
        if (bk && bk->shelf >= 0) {
            p3 = bookPos(*bk, bk->shelf, bk->xT);
            out = shelves_[bk->shelf].out;
        }
        const float s = saturate(fl.t / fl.dur);
        const float L = length(p3 - fl.p0);
        const vec3 pA = p3 + out * (bk ? std::max(0.12f, bk->d * 1.4f) : 0.f);
        const vec3 p2 = pA + UP * (0.18f * L) + out * (0.25f * L);
        const float sb = std::min(1.f, s / 0.86f);
        const float e = 1.f - std::pow(1.f - sb, 2.f);
        const float u = 1.f - e;
        vec3 p = fl.p0 * (u * u * u) + fl.p1 * (3.f * u * u * e) + p2 * (3.f * u * e * e) + pA * (e * e * e);
        if (s > 0.86f) p = lerp(pA, p3, smoothstepf(0.86f, 1.f, s));
        const float mk = smoothstepf(0.05f, 0.75f, s);
        const vec3 dims = lerp(fl.dims0, fl.dims1, mk);
        vec3 face = normalize(lerp(fl.face0, out, smoothstepf(0.15f, 0.8f, s)));
        const float sp = fl.spin * TAU * smoothstepf(0.1f, 0.75f, s);
        if (sp != 0.f) {
            const float c = std::cos(sp), sn = std::sin(sp);
            face = normalize(vec3(face.x * c - face.z * sn, face.y, face.x * sn + face.z * c));
        }
        const vec3 Zd = normalize(vec3(face.x, 0.f, face.z) + vec3(0.f, face.y, 0.f));
        const vec3 Xd = normalize(cross(UP, Zd));
        const vec3 Yd = cross(Zd, Xd);
        const float glow = 0.12f;
        putBook(flightInst_, p, Xd * dims.x, Yd * dims.y, Zd * dims.z, fl.type, fl.style, glow, fl.gilt, fl.b0.x * 0.3f, -100.f);
        if (s >= 1.f) {
            if (bk) {
                bk->flying = false;
                bk->glow = 0.35f;
                bk->shelfShown = bk->shelf;
                bk->x = bk->xT;
                bk->hop = 1.f;
            }
            it = flights_.erase(it);
            continue;
        }
        ++it;
    }
    buildBoard(game);

    // ---- Lamps: their glows and the 16 that light the scene.
    const float lampLevel = pal_.lampK * (0.8f + 0.1f * wm + 0.2f * wp + 0.25f * kickK_ * wp + 0.25f * dropAmt_) * (1.f - 0.35f * slow_);
    const float flick = pal_.flicker;
    std::vector<vec4> LP, LC;
    {
        std::vector<std::pair<float, int>> cand;
        const float reach = camD_ * 3.f + 10.f;
        for (int i = 0; i < (int)lamps_.size(); i++) {
            const Lamp& l = lamps_[i];
            const float on = saturate((grow_ + 0.6f - l.born) / 0.6f) * (l.die > 0.f ? 1.f - saturate((grow_ - l.die) / 0.6f) : 1.f);
            if (on <= 0.f) continue;
            const float d = length(l.p - eyeW_);
            if (d > reach * 2.f) continue;
            const float fl = l.flick > 0.f ? 1.f + l.flick * flick * (0.25f * std::sin(worldTime_ * 11.f + i * 3.1f) + 0.15f * std::sin(worldTime_ * 23.f + i)) : 1.f;
            const float k = l.k * lampLevel * on * fl;
            sprites_.push_back({{l.p.x, l.p.y, l.p.z, l.r}, {l.col.x, l.col.y, l.col.z, k}});
            if (l.range > 0.f) {
                const float dt2 = length(l.p - targetW_);
                cand.push_back({k * l.range / (1.f + dt2 / std::max(2.f, camD_ * 0.5f)), i});
            }
        }
        std::sort(cand.begin(), cand.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
        for (int j = 0; j < (int)cand.size() && j < 16; j++) {
            const Lamp& l = lamps_[cand[j].second];
            const float on = saturate((grow_ + 0.6f - l.born) / 0.6f) * (l.die > 0.f ? 1.f - saturate((grow_ - l.die) / 0.6f) : 1.f);
            const float fl = l.flick > 0.f ? 1.f + l.flick * flick * (0.25f * std::sin(worldTime_ * 11.f + cand[j].second * 3.1f)) : 1.f;
            LP.push_back(vec4(l.p, l.range));
            LC.push_back(vec4(l.col, l.k * lampLevel * on * fl * 1.4f));
        }
    }
    // dust motes in the lamplight: calm parts mostly; they hang still in slow motion
    {
        const float calm = (1.f - 0.7f * wp) * (0.6f + 0.4f * (1.f - wm));
        const vec3 size(camD_ * 1.1f, camD_ * 0.6f, camD_ * 0.7f);
        const vec3 org = targetW_ - vec3(size.x * 0.5f, size.y * 0.4f, size.z * 0.3f);
        const vec3 mc = lerp(pal_.lampCol, vec3(1.f), 0.4f);
        for (const Mote& m : motes_) {
            vec3 q = m.p + vec3(std::sin(worldTime_ * 0.05f + m.seed * 30.f) * 0.03f, std::fmod(worldTime_ * 0.006f * (0.5f + m.seed) + 10.f, 1.f),
                                std::cos(worldTime_ * 0.04f + m.seed * 17.f) * 0.03f);
            q.y = q.y - std::floor(q.y);
            const vec3 p = org + vec3(q.x * size.x, q.y * size.y, q.z * size.z);
            const float tw = 0.5f + 0.5f * std::sin(worldTime_ * (1.f + m.seed * 2.f) + m.seed * 40.f);
            sprites_.push_back({{p.x, p.y, p.z, camD_ * 0.0022f}, {mc.x, mc.y, mc.z, calm * (0.25f + 0.55f * tw) * (pal_.night > 0.5f ? 1.f : 0.7f)}});
        }
    }

    // ---- Uniforms shared by the passes.
    const Palette& P = pal_;
    const float stageIn = 0.f;
    const vec3 keyDir = normalize(lerp(VARIANTS[variant_].keyDir, vec3(0.15f, 1.f, 0.25f), stageIn * 0.7f));
    const float keyK = (1.f + 0.1f * wp + 0.1f * dropAmt_) * (1.f - 0.5f * slow_);
    const vec3 slowTint(0.75f, 0.85f, 1.15f);
    auto lighting = [&](GLuint p, bool board) {
        if (board) {
            set3f(p, "uAmbSky", lerp(P.ambSky, vec3(0.5f), 0.35f) * 0.9f);
            set3f(p, "uAmbGround", P.ambGround * 0.8f + vec3(0.03f));
            set3f(p, "uKeyDir", normalize(vec3(-0.45f, 0.7f, 0.65f)));
            set3f(p, "uKeyCol", lerp(P.lampCol, vec3(1.f), 0.45f) * (1.6f + 0.15f * wp + 0.3f * dropAmt_) * lerp(vec3(1.f), slowTint, slow_));
            set1i(p, "uNL", 0);
            set1f(p, "uShadowOn", 0.f);
            set1f(p, "uFogD", 0.f);
        } else {
            // the lamps' warm fill on top of the sky's light (most of the light at night)
            const vec3 warm = P.lampCol * (0.015f + 0.085f * P.night) * lampLevel;
            set3f(p, "uAmbSky", P.ambSky * (1.f - 0.3f * slow_) + warm);
            set3f(p, "uAmbGround", P.ambGround + warm * 0.7f);
            set3f(p, "uKeyDir", keyDir);
            set3f(p, "uKeyCol", P.key * keyK * lerp(vec3(1.f), slowTint, slow_));
            const int n = (int)LP.size();
            set1i(p, "uNL", n);
            if (n) {
                glUniform4fv(ugl::U(p, "uLP"), n, &LP[0].x);
                glUniform4fv(ugl::U(p, "uLC"), n, &LC[0].x);
            }
            set1f(p, "uShadowOn", 1.f);
            // the haze: barely any in the rooms, the abyss' depth once the shaft opens
            const float fogK = lerpf(0.1f, 1.f, stageIn);
            set1f(p, "uFogD", fogK * 0.5f / std::max(4.f, camD_ * 2.5f));
            set1f(p, "uFogStart", camD_ * lerpf(0.9f, 0.45f, stageIn));
            set1f(p, "uFogMid", targetW_.y);
            set1f(p, "uFogSpan", camD_ * 0.9f);
            set3f(p, "uFogLow", lerp(P.fogLow, P.fogLow * 0.5f, stageIn));
            set3f(p, "uFogHigh", P.fogHigh);
        }
    };
    auto common = [&](GLuint p, bool board) {
        set3f(p, "uEye", board ? eyeB_ : eyeW_);
        set1f(p, "uTime", worldTime_);
        setMat(p, "uVP", board ? vpB_ : vpW_);
        set3f(p, "uOrigin", board ? vec3(0.f) : eyeW_);
        set1f(p, "uGrow", board ? 1e6f : grow_ + 0.86f);
        set1f(p, "uSpan", 0.85f);
        set1f(p, "uLift", 45.f);
    };

    // ---- The key light's shadow map, round what the camera sees.
    const vec3 shC = targetW_;
    const float shR = std::max(5.f, camD_ * 1.25f);
    mat4 shVP;
    {
        const mat4 view = mat4::lookAt(keyDir * (shR * 2.f), vec3(0.f), std::fabs(keyDir.y) < 0.95f ? UP : vec3(1, 0, 0));
        shVP = mat4::ortho(-shR, shR, -shR, shR, 0.01f * shR, shR * 4.f) * view;
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glViewport(0, 0, 2048, 2048);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glUseProgram(progShadow_);
        setMat(progShadow_, "uVP", shVP);
        set3f(progShadow_, "uOrigin", shC);
        set1f(progShadow_, "uGrow", grow_ + 0.86f);
        set1f(progShadow_, "uSpan", 0.85f);
        set1f(progShadow_, "uLift", 45.f);
        set4f(progShadow_, "uCut", cut_.x, cut_.y, cut_.z, cut_.w);
        for (int m = 0; m < MESH_COUNT; m++) {
            if (statCount_[m]) {
                glBindVertexArray(statVao_[m]);
                glDrawElementsInstanced(GL_TRIANGLES, meshN_[m], GL_UNSIGNED_INT, nullptr, (GLsizei)statCount_[m]);
            }
            if (!dyn_[m].empty()) {
                glBindVertexArray(dynVao_[m]);
                glBindBuffer(GL_ARRAY_BUFFER, dynVbo_[m]);
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(dyn_[m].size() * sizeof(Inst)), dyn_[m].data(), GL_STREAM_DRAW);
                glDrawElementsInstanced(GL_TRIANGLES, meshN_[m], GL_UNSIGNED_INT, nullptr, (GLsizei)dyn_[m].size());
            }
        }
    }

    // ---- The scene (multisampled): the library, its glows, the veil, the flights, then the well.
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    const vec3 bg = lerp(P.fogLow, P.fogHigh, 0.35f);
    glClearColor(bg.x, bg.y, bg.z, 0.f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(progSolid_);
    common(progSolid_, false);
    lighting(progSolid_, false);
    set1f(progSolid_, "uBoard", 0.f);
    set1f(progSolid_, "uStackA", 0.f);
    set4f(progSolid_, "uCut", cut_.x, cut_.y, cut_.z, cut_.w);
    {
        std::vector<vec3> fill(P.fill, P.fill + 5), rain(P.rain, P.rain + 7);
        glUniform3fv(ugl::U(progSolid_, "uFill"), 5, &fill[0].x);
        glUniform3fv(ugl::U(progSolid_, "uRain"), 7, &rain[0].x);
    }
    set3f(progSolid_, "uGilt", P.gilt);
    set3f(progSolid_, "uPaper", P.paper);
    set3f(progSolid_, "uSkyA", lerp(P.skyA, P.skyA * slowTint * 0.5f, slow_) * (P.night > 0.5f ? 0.9f : 2.2f));
    set3f(progSolid_, "uSkyB", P.skyB * (P.night > 0.5f ? 0.9f : 2.2f));
    set3f(progSolid_, "uSection", P.section);
    set1f(progSolid_, "uLampK", lampLevel);
    set1f(progSolid_, "uNight", P.night);
    set1f(progSolid_, "uFlicker", flick);
    set1f(progSolid_, "uMoon", P.moon);
    set1f(progSolid_, "uSortRate", 0.55f);
    set3f(progSolid_, "uWaveO", waveO_);
    set1f(progSolid_, "uWaveT", 99.f);
    set1f(progSolid_, "uWaveSpeed", std::max(4.f, camD_ * 0.9f));
    set1f(progSolid_, "uGlowAll", 0.f);
    set1f(progSolid_, "uBeat", beatPulse_);
    set1f(progSolid_, "uWorld", 1.f);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    set1i(progSolid_, "uShadow", 1);
    setMat(progSolid_, "uShadowVP", shVP);
    set3f(progSolid_, "uShadowO", shC);
    set1f(progSolid_, "uShadowTexel", 1.f / 2048.f);
    set1f(progSolid_, "uShadowBias", 2.f * shR / 2048.f * 1.5f);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sortTex_);
    set1i(progSolid_, "uSortTex", 2);
    glActiveTexture(GL_TEXTURE0);
    for (int m = 0; m < MESH_COUNT; m++) {
        if (statCount_[m]) {
            glBindVertexArray(statVao_[m]);
            glUniform1i(ugl::U(progSolid_, "uMesh"), m);
            glDrawElementsInstanced(GL_TRIANGLES, meshN_[m], GL_UNSIGNED_INT, nullptr, (GLsizei)statCount_[m]);
        }
        if (!dyn_[m].empty()) drawInst(dynVao_[m], dynVbo_[m], m, nullptr, dyn_[m].size());
    }
    // the glows (no alpha: the frame's alpha is the stack's)
    glEnable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glDepthMask(GL_FALSE);
    if (!sprites_.empty()) {
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(progSprite_);
        set3f(progSprite_, "uEye", eyeW_);
        set1f(progSprite_, "uTime", worldTime_);
        setMat(progSprite_, "uVP", vpW_);
        set3f(progSprite_, "uOrigin", eyeW_);
        set3f(progSprite_, "uCamR", rightW_);
        set3f(progSprite_, "uCamU", upW_);
        set1f(progSprite_, "uMinR", 1.4f * 2.f * tanW_ / height_);
        set1f(progSprite_, "uFogD", lerpf(0.1f, 1.f, stageIn) * 0.5f / std::max(4.f, camD_ * 2.5f));
        set1f(progSprite_, "uFogStart", camD_ * 0.6f);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sprites_.size() * sizeof(Sprite)), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    // the veil behind the well and the HUD's columns
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progVeil_);
    set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progVeil_, "uVeil", P.veil);
    set1f(progVeil_, "uWellA", P.night > 0.5f ? 0.74f : 0.78f);
    set1f(progVeil_, "uColA", P.night > 0.5f ? 0.45f : 0.68f);
    set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
    set4f(progVeil_, "uColR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    ugl::drawFullscreen();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_BLEND);
    // the flights over the veil (they still meet the shelves' depth)
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    if (!flightInst_.empty()) {
        glUseProgram(progSolid_);
        set1f(progSolid_, "uStackA", 0.f);
        drawInst(dynVao_[MESH_BOX], dynVbo_[MESH_BOX], MESH_BOX, &flightInst_, flightInst_.size());
    }
    // ---- The well: its bookcase frame, the books, the ghost
    glClear(GL_DEPTH_BUFFER_BIT);
    {
        std::vector<Inst> frame;
        auto fb = [&](const vec3& c, const vec3& h, const vec3& col, int mat, float p0 = 0.f) {
            Inst in{};
            in.c[0] = c.x; in.c[1] = c.y; in.c[2] = c.z; in.c[3] = (float)mat;
            in.x[0] = h.x; in.x[3] = p0;
            in.y[1] = h.y;
            in.z[2] = h.z;
            in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = 0.4f;
            in.e[0] = -100.f; in.e[1] = -1.f; in.e[2] = -1.f;
            frame.push_back(in);
        };
        const vec3 wd = P.wood * 0.85f;
        for (int s = -1; s <= 1; s += 2) {
            fb(vec3(s * 5.2f, -0.1f, -0.42f), vec3(0.2f, 10.65f, 0.45f), wd, M_WOOD);
            fb(vec3(s * 5.06f, -0.1f, 0.06f), vec3(0.06f, 10.65f, 0.04f), P.gilt * 0.6f, M_BRASS);
        }
        fb(vec3(0.f, -10.62f, -0.4f), vec3(5.42f, 0.13f, 0.5f), wd * 0.9f, M_WOOD, 1.f);
        fb(vec3(0.f, -10.82f, -0.3f), vec3(5.55f, 0.1f, 0.6f), wd * 0.8f, M_WOOD);
        glUseProgram(progSolid_);
        common(progSolid_, true);
        lighting(progSolid_, true);
        set1f(progSolid_, "uBoard", 1.f);
        set4f(progSolid_, "uCut", 0.f, 0.f, 0.f, 1e9f);
        set1f(progSolid_, "uWorld", 0.25f);
        set1f(progSolid_, "uStackA", 0.f);
        drawInst(dynVao_[MESH_BOX], dynVbo_[MESH_BOX], MESH_BOX, &frame, frame.size());
        set1f(progSolid_, "uStackA", 1.f);
        if (!cells_.empty()) drawInst(dynVao_[MESH_BOX], dynVbo_[MESH_BOX], MESH_BOX, &cells_, cells_.size());
    }
    if (!cellLines_.empty()) {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(progLine_);
        setMat(progLine_, "uVP", vpB_);
        set2f(progLine_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(cellLines_.size() * sizeof(Line)), cellLines_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cellLines_.size());
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_BLEND);
    }

    // ---- Resolve, the bloom, the frame.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glDisable(GL_DEPTH_TEST);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    set1f(progDown_, "uThr", 0.7f);
    ugl::drawFullscreen();
    glUseProgram(progBlur_);
    set1i(progBlur_, "uSrc", 0);
    for (int i = 0; i < 2; i++) {
        const float stp = 1.f + i;
        bloomB_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
        set2f(progBlur_, "uDir", stp / bloomA_.w, 0.f);
        ugl::drawFullscreen();
        bloomA_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
        set2f(progBlur_, "uDir", 0.f, stp / bloomA_.h);
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
    set1f(progFinal_, "uBloomK", (0.6f + 0.1f * wp + 0.12f * dropAmt_) * (1.f + 0.4f * slow_));
    set1f(progFinal_, "uExposure", P.exposure);
    set1f(progFinal_, "uSatF", 1.04f + 0.06f * wp + 0.08f * dropAmt_);
    set1f(progFinal_, "uSlow", slow_ * 0.5f);
    set3f(progFinal_, "uSlowTint", slowTint);
    set3f(progFinal_, "uLift", vec3(0.f));
    set1f(progFinal_, "uVig", 0.35f);
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
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_library() {
    static const UniverseEntry e{
        "LIBRARY", "the blocks are books, put away sorted on the shelves of a personal library growing from a cosy study into a long room of bookcases, in 3D",
        [] { return std::unique_ptr<Universe>(new LibraryUniverse()); },
        // A quiet, warm world: suits calm and mid-tempo songs best; fits most.
        [](const SongLook& l) { return 0.8f + 0.2f * (1.f - l.density) + 0.1f * (1.f - l.bassWeight); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
