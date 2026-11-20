// LIBRARY: the blocks are books, and every cleared line brings three big books to a small, cosy library room that
// fills with the game (complete at about 50 lines, one song).
//
// The board stays a clean game board over a deep saturated ground, its cells a packed shelf: each cell a row of book
// spines seen from the front in its piece's color family (three or four standing, a big one beside a slim one, or a
// pile of three lying; each book its own shade, a pale label or thin bands, thin dark gaps between them: the shader's
// shelfCell), and each cell keeps its books through falls and clears (its seed follows it). When a row
// clears, its blocks dissolve into light where they stand, and three of its cells send a book to the room: from behind
// the cells' places on the board, at a depth behind the well, only ever going deeper (never in front of the board or
// the HUD), turning over once on the way. Each lands in the next free place (the order of the runs in world.cpp) with
// a soft glow and never moves again, so the room is the history of the game.
//
// The room is a diorama seen from above through a long lens, drawn as a toon picture book (Link's Awakening): a
// plank floor on a thick slab, walls cut in section, two big bookcases, round windows, a reading nook, a big plant;
// the ladder, the piles, the mezzanine and more bookcases grow in from their floors as the collection needs them.
// Calm and lower in contrast than the board; cel shading in bands with a cool colored shade, inked edges.
//
// The song: dust in the light; the lamps follow the music; in a real drop the shelves breathe (a wave a beat rolls
// along each shelf, sliding the books out, tipped forward, and back into their places), loose pages flutter up from
// the bookcases and a soft light runs along the collection; a quad runs one light along it. Slow motion: blue hour.
// Game over: the blocks topple out of the bottom of the well. Around a song the camera comes in to the dim room as
// its lamps come on one by one (the setup), and draws back as they go out one by one (the teardown).
//
// Layout contract: the board is drawn through the reference camera raised a little and shifted back down (the board
// plane keeps its pixels, the books' tops show); the room has its own camera behind it.
#include "universes/library/library.hpp"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "layout.hpp"
#include "universes/library/shaders.hpp"
#include "universes/library/world.hpp"
#include "universes/registry.hpp"

using namespace lib_world;

namespace {

// Instrument Sans (Regular, SemiBold, Italic), SIL Open Font License, see OFL-InstrumentSans.txt.
const unsigned char FONT_REG[] = {
#include "universes/library/InstrumentSans-Regular.ttf.inc"
};
const unsigned char FONT_SEMI[] = {
#include "universes/library/InstrumentSans-SemiBold.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/library/InstrumentSans-Italic.ttf.inc"
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
uint32_t hash32(uint64_t x) { return (uint32_t)(splitmix64(x) >> 32); }

// A variant (sRGB), one mood of the same cosy room: the seven bindings (I O T S Z J L, the board's; the shelved books
// are these, muted); the wood (bookcases, furniture), the bookcases' backs, the walls, their wainscot, the walls'
// cut section, the floor's two planks, the metal, the paper; the sky (high, low: the backdrop and the windows); the
// shade's cool light (from the sky, from the ground), the sun, the lamps; the armchair, the rug (two tones), the
// leaves, the pots; the well's ground (low, high); the HUD's card, its edge, its ink, accent and halo; the ink lines,
// the warm rim; the clouds (lit, shaded).
struct VariantDef {
    const char* name;
    unsigned piece[7];
    unsigned oak, bay, wall, wall2, cut, floor, floorB, steel, paper, skyA, skyB, ambSky, ambGround, key, led;
    unsigned chair, rug, rug2, leaf, pot, veil, veilTop, card, cardEdge, ink, accent, halo, line, rim, cloud, cloudShade;
    float night, keyK, ambK, ledK, exposure, floorKind;
    vec3 keyDir;
    bool light;
};
const VariantDef VARIANTS[] = {
    // golden afternoon: warm cream walls over an apricot wainscot, honey floor, a low golden sun, lavender shade; coral
    {"GOLDEN HOUR",
     {0x22d0f0, 0xffd31a, 0xb45cff, 0x4fd24a, 0xff4b4b, 0x3f7dff, 0xff9620},
     0xc98450, 0xf2d4a8, 0xf6e4c8, 0xeab07c, 0x7a3d62, 0xd99c60, 0xe2a96c, 0x3d5a6e, 0xfff1d8, 0x7fb6ee, 0xffd2a0,
     0xbcc4f4, 0xf2c4b0, 0xffc878, 0xffd27a,
     0x2f9e8f, 0xd8604e, 0xf2b75a, 0x4caf50, 0xd8643c, 0x1d1e4c, 0x2e3274, 0xfff3dc, 0xff7a4d, 0x26203c, 0xe8473c, 0xfff3dc,
     0x3a1f40, 0xffcf86, 0xffffff, 0xf6c2c8,
     0.f, 0.65f, 0.85f, 0.8f, 1.0f, 1.f, vec3(-0.55f, 0.6f, 0.55f), true},
    // sunny noon: soft white walls over a sky-blue wainscot, light wood, a high white sun; cobalt
    {"SUNNY NOON",
     {0x1ccff5, 0xffd60f, 0xa855ff, 0x3fcf45, 0xff4545, 0x3a78ff, 0xff9418},
     0xe0a85c, 0xfbf0dc, 0xeef3f6, 0xa6d4e6, 0x2c4f8a, 0xe8c48c, 0xf0d29e, 0x2350c8, 0xfffaf0, 0x3d8ef0, 0xbfe6ff,
     0xc0d0ff, 0xd8d0f4, 0xfff4e0, 0xfff0c8,
     0xf2c53d, 0x3d7be0, 0xa8d8f0, 0x3dbb50, 0xff7a4a, 0x162452, 0x23397c, 0xffffff, 0x2f86ff, 0x1a2140, 0x2a6cf0, 0xffffff,
     0x1a2050, 0xfff0b0, 0xffffff, 0xc6dbff,
     0.f, 0.58f, 0.8f, 0.6f, 1.0f, 1.f, vec3(-0.3f, 0.85f, 0.42f), true},
    // fresh morning: mint walls, butter floor, coral wood, a pink dawn sky, a cool soft shade; pink
    {"MORNING",
     {0x26c9f0, 0xffcf24, 0xae5cf6, 0x48cc56, 0xff4d5e, 0x4479ff, 0xff8f2a},
     0xe39a78, 0xfff1e2, 0xe2f1ea, 0x96d3bf, 0xc9506a, 0xf0d6a2, 0xf6e0b4, 0xe0566a, 0xfff8ec, 0x6fb8ff, 0xffdcea,
     0xc0d8ff, 0xb8e4ec, 0xfff0dc, 0xfff2d0,
     0xa57be0, 0xf08aa0, 0xfff0c0, 0x52c46a, 0xf2765a, 0x152e4c, 0x1f4a70, 0xfffaf2, 0x2fc2a4, 0x1b2a3a, 0xe8506a, 0xfffaf2,
     0x1e2a4a, 0xffe0cc, 0xffffff, 0xf6cce0,
     0.f, 0.58f, 0.82f, 0.6f, 1.0f, 1.f, vec3(-0.35f, 0.75f, 0.55f), true},
    // starry night: indigo walls, honey bookcases glowing under warm round lamps, stars in the windows; gold
    {"STARRY NIGHT",
     {0x2ad6f5, 0xffd42a, 0xc06bff, 0x56dc58, 0xff5a5a, 0x5a8cff, 0xffa030},
     0xc98048, 0x2c3270, 0x37428a, 0x2a3270, 0x141838, 0x3e3c7e, 0x48468c, 0xffb84a, 0xfff0d0, 0x0c1240, 0x2c3c90,
     0x4a52c0, 0x5a3c6a, 0x9ab0ff, 0xffc46a,
     0xe0603a, 0xb84a6a, 0xffbe4a, 0x2f9a5a, 0xd8603a, 0x0a0c26, 0x151b46, 0x1e2558, 0xffb84a, 0xfff2dc, 0xffbe4a, 0x10143a,
     0x08081e, 0xffc070, 0x7c8cd4, 0x46549c,
     1.f, 0.35f, 0.95f, 2.1f, 1.0f, 1.f, vec3(-0.5f, 0.7f, 0.5f), false},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The board's camera: closer than the reference one (a real perspective: the books' tops show, more the lower they are;
// the columns off the middle show their inner sides) and above the well's middle, its image shifted so the board plane
// keeps the reference pixels exactly (the cells stay the game's cells).
constexpr float BOARD_EYE_Y = 15.f;  // its height (above the top row: even the falling piece shows its books' tops)
constexpr float BOARD_EYE_D = 22.f;  // its distance from the board plane (the reference's is some 35)
constexpr float FX_T = 0.27f;        // a cleared row dissolves in this long (the stack above waits 0.3 s)

// Test hooks: REFALL_LIBRARY_LINES=N adds N lines (their books already in place: for screenshots),
// REFALL_GROW_RATE=R adds R lines a second (each one shelved like a real line: its books fly to their places).
// Each cleared line brings three big books at the reference pace (so some 50 lines, one song of 3:20, fill the room),
// launched from three of the row's cells (1, 5, 8). The pace follows the song's length (sceneLines()): a line brings
// BOOKS_LINE x 50 / sceneLines() books on average (1.5 in a 100-line scene, 7.5 in a 20-line one; a fraction carried
// to the next line), each from its own cell of the row, so every book still comes from a cleared line.
constexpr int BOOKS_LINE = 3;
// The k cells of a row that send its books, spread along it (k = 3: cells 1, 5 and 8).
bool bookCell(int x, int k) {
    for (int j = 0; j < k; j++)
        if (x == (int)((j + 0.5f) * Game::W / k)) return true;
    return false;
}

int startLines() {
    static const int n = std::getenv("REFALL_LIBRARY_LINES") ? std::atoi(std::getenv("REFALL_LIBRARY_LINES")) : 0;
    return n;
}
int rateLines(double t) {
    static const double rate = std::getenv("REFALL_GROW_RATE") ? std::atof(std::getenv("REFALL_GROW_RATE")) : 0.0;
    return (int)(rate * t);
}

float easeOutBack(float t) {
    t = saturate(t);
    const float c = 1.4f;
    const float u = t - 1.f;
    return 1.f + (c + 1.f) * u * u * u + c * u * u;
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

    // ---- The meshes: a box, a cylinder, a hemisphere (position + normal, indexed).
    {
        std::vector<float> v[MESH_COUNT];
        std::vector<unsigned> idx[MESH_COUNT];
        auto vert = [&](int m, const vec3& p, const vec3& n) {
            v[m].insert(v[m].end(), {p.x, p.y, p.z, n.x, n.y, n.z});
            return (unsigned)(v[m].size() / 6 - 1);
        };
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
        {
            const int S = 24;
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
        {
            const int LA = 12, LO = 32;
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
        glGenVertexArrays(1, &shelfVao_);
        glGenBuffers(1, &shelfVbo_);
        setupInstVao(shelfVao_, shelfVbo_, MESH_BOX);
        glGenVertexArrays(1, &glassVao_);
        glGenBuffers(1, &glassVbo_);
        setupInstVao(glassVao_, glassVbo_, MESH_BOX);
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
    glBindVertexArray(0);

    // ---- The sun's shadow map.
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

    resize(width, height);
    std::memset(typeGrid_, -1, sizeof(typeGrid_));
    // dust: scattered in a unit box, placed around the camera's target as it moves
    motes_.clear();
    Rng r(0x5EED);
    for (int i = 0; i < 220; i++) motes_.push_back({vec3(r.uniform(), r.uniform(), r.uniform()), r.uniform()});
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

// The board: a camera at BOARD_EYE_D from the board plane (its field widened so that plane keeps the reference's
// scale), raised to BOARD_EYE_Y and looking straight ahead, its image shifted back down so the board plane keeps the
// reference pixels.
void LibraryUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    aspect_ = (float)width_ / height_;
    const layout::Camera ref = layout::camera(width_, height_);
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    const float focal = 1.f / std::tan(half);
    const float eyeD = std::min(BOARD_EYE_D, ref.dist);
    eyeB_ = vec3(ref.target.x, BOARD_EYE_Y, ref.target.z + eyeD);
    const mat4 view = mat4::lookAt(eyeB_, eyeB_ + vec3(0.f, 0.f, -1.f), UP);
    mat4 shift = mat4::identity();
    shift.at(1, 3) = focal * (BOARD_EYE_Y - ref.target.y) / ref.dist;
    const float halfB = std::atan(std::tan(half) * ref.dist / eyeD);
    vpB_ = shift * mat4::perspective(2.f * halfB, aspect_, 0.5f, 400.f) * view;
    const float hh = ref.dist * std::tan(half);
    viewH_ = vec2{hh * aspect_, hh};
    viewC_ = vec2{ref.target.x, ref.target.y};

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
    for (GLuint p : {progSolid_, progShadow_, progSprite_, progLine_, progVeil_, progDown_, progBlur_, progFinal_, progShape_})
        glDeleteProgram(p);
    for (int m = 0; m < MESH_COUNT; m++) {
        glDeleteVertexArrays(1, &statVao_[m]);
        glDeleteVertexArrays(1, &dynVao_[m]);
        glDeleteBuffers(1, &statVbo_[m]);
        glDeleteBuffers(1, &dynVbo_[m]);
        glDeleteBuffers(1, &meshVbo_[m]);
        glDeleteBuffers(1, &meshIbo_[m]);
    }
    for (GLuint v : {spriteVao_, lineVao_, shapeVao_, glassVao_, shelfVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, lineVbo_, shapeVbo_, glassVbo_, shelfVbo_}) glDeleteBuffers(1, &b);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    if (shadowTex_) glDeleteTextures(1, &shadowTex_);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

vec3 LibraryUniverse::pieceColor(int type) const { return pal_.piece[std::clamp(type, 0, 6)]; }

void LibraryUniverse::setSong(const SongLook& t, float seconds) {
    (void)seconds;
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = 3;              // evening
        else if (t.hasSong && !t.minor && t.brightness > 0.6f) v = 1;         // white cube
        else if (t.hasSong && (t.airWeight > 0.58f || t.dynamics < 0.35f)) v = 2; // birch
        else if (t.hasSong) v = 0;                                            // daylight
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    const bool changed = v != variant_ || !colSet_;
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    Palette& P = pal_;
    // the pieces a gentle step toward pastel (lifted toward white in sRGB, a hair less saturated): same hues, still
    // bright against the deep well, just not flashy
    for (int i = 0; i < 7; i++) {
        const vec3 c = hex(V.piece[i]);
        const float l = dot(c, vec3(0.299f, 0.587f, 0.114f));
        P.piece[i] = ugl::toLinear(lerp(lerp(c, vec3(l), 0.08f), vec3(1.f), 0.17f));
    }
    P.oak = lin(V.oak); P.bay = lin(V.bay); P.wall = lin(V.wall); P.wall2 = lin(V.wall2); P.cut = lin(V.cut); P.floor = lin(V.floor);
    P.floorB = lin(V.floorB); P.steel = lin(V.steel); P.paper = lin(V.paper); P.skyA = lin(V.skyA); P.skyB = lin(V.skyB);
    P.ambSky = lin(V.ambSky) * V.ambK; P.ambGround = lin(V.ambGround) * V.ambK; P.key = lin(V.key) * V.keyK; P.led = lin(V.led);
    P.chair = lin(V.chair); P.rug = lin(V.rug); P.rug2 = lin(V.rug2); P.leaf = lin(V.leaf); P.pot = lin(V.pot);
    P.veil = lin(V.veil); P.veilTop = lin(V.veilTop); P.card = lin(V.card); P.cardEdge = lin(V.cardEdge); P.line = lin(V.line);
    P.rim = lin(V.rim); P.cloud = lin(V.cloud); P.cloudShade = lin(V.cloudShade);
    P.ink = hex(V.ink); P.accent = hex(V.accent); P.halo = hex(V.halo);
    P.night = V.night; P.keyK = V.keyK; P.ledK = V.ledK; P.exposure = V.exposure; P.floorKind = V.floorKind; P.light = V.light;
    colSet_ = true;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x11B7ull);
    if (changed) {
        worldDirty_ = true;
        shelfInst_.clear(); // (their colors are baked in)
        settled_ = 0;
        shelfDirty_ = true;
    }
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
    h.background = pal_.light ? lerp(pal_.wall, pal_.oak, 0.25f) : lerp(pal_.skyB, pal_.floor, 0.5f) * 0.6f;
    h.light = pal_.light;
    h.ink = pal_.light ? lin(0x2a2c30) : lin(0xece6da);
    h.accent = ugl::toLinear(pal_.accent);
    h.styles = TS_REBUILD | TS_TILES | TS_CURTAIN;
    return h;
}

// =====================================================================================================
// The board's cells
// =====================================================================================================

// A cell of the board: a bit of a packed shelf, two or three books standing on their spines, flush at the front, each
// its own width, height, depth, shade and spine design, against a dark shelf back (seen in the thin gaps between
// them, and above a shorter one). They are the room's books (the same construction and shading, M_BOOK: the spine's
// designs, the boards round a cream page block that shows on their tops, seen a little from above), in their piece's
// full color and a little stockier, closer to a cube, so they fill the cell. All from the cell's seed: a cell keeps its
// books through falls and clears. along / up / out: its frame (a toppling cell turns it), k its size, fade its
// dissolve into light (0 none .. 1 gone), covered: a cell above it (its books' tops in its shade, toward the back).
void LibraryUniverse::pushCell(std::vector<Inst>& out, int type, const vec3& c, const vec3& along, const vec3& up, float k, float fade,
                               uint32_t seed, bool covered) {
    const float sw = k * (1.f + 0.05f * fade);
    const vec3 outD = cross(along, up);
    const float H = 0.468f * sw;          // the cell's half side
    const float D = 0.62f * sw;           // the deepest book (the shelf back just behind it)
    const vec3 col = pieceColor(type);
    auto put = [&](const vec3& cc, const vec3& X, const vec3& Y, const vec3& Z, const vec3& cl, int mat, float p0, float sd) {
        Inst in;
        in.c[0] = cc.x; in.c[1] = cc.y; in.c[2] = cc.z; in.c[3] = (float)mat;
        in.x[0] = X.x; in.x[1] = X.y; in.x[2] = X.z; in.x[3] = p0;
        in.y[0] = Y.x; in.y[1] = Y.y; in.y[2] = Y.z; in.y[3] = 0.f;
        in.z[0] = Z.x; in.z[1] = Z.y; in.z[2] = Z.z; in.z[3] = fade;
        in.col[0] = cl.x; in.col[1] = cl.y; in.col[2] = cl.z; in.col[3] = sd;
        in.e[0] = -100.f; in.e[1] = -1.f; in.e[2] = -1.f; in.e[3] = covered ? -20.f : -10.f;
        out.push_back(in);
    };
    // the books, left to right: two (more often) or three, of uneven widths; a little short of the cell's height (seen
    // from a little above, their tops show under the cell above: the cream page block between the boards), now and
    // then a shorter one; a little uneven in depth
    Rng r(0x5E1F0ull ^ ((uint64_t)seed * 2654435761ull));
    const int n = r.chance(0.6f) ? 2 : 3;
    float w[3], tot = 0.f;
    for (int i = 0; i < n; i++) tot += (w[i] = r.range(0.78f, 1.22f));
    const float gap = 0.018f * sw;
    float x0 = -H, top = 0.f;
    for (int i = 0; i < n; i++) {
        const float bw = (2.f * H - gap * (n - 1)) * w[i] / tot;
        const float bh = 2.f * H * (r.chance(0.2f) ? r.range(0.72f, 0.8f) : r.range(0.84f, 0.92f));
        const float bd = D * r.range(0.84f, 1.f);
        // (the room's spine designs; more of the labels, bands and caps)
        const int style = (int)"0001144423"[r.next() % 10] - '0';
        const vec3 bc = col * r.range(0.9f, 1.07f);
        const float bs = (float)(r.next() % 1000) / 97.f;
        put(c + along * (x0 + bw * 0.5f) + up * (bh * 0.5f - H) - outD * (bd * 0.5f), along * (bw * 0.5f), up * (bh * 0.5f),
            outD * (bd * 0.5f), bc, M_BOOK, (float)style, bs);
        x0 += bw + gap;
        top = std::max(top, bh);
    }
    // the shelf back, behind them, as high as the tallest (above it, the well's ground or the HUD's card)
    const float bt = 0.03f * sw;
    put(c + up * (top * 0.5f - H) - outD * (D + bt), along * H, up * (top * 0.5f), outD * bt, col, M_CELL, 0.f, 0.f);
}

uint32_t LibraryUniverse::cellSeed(int x, int y, int type) {
    if (y < 0 || y >= Game::H || x < 0 || x >= Game::W) return hash32((uint64_t)(x * 131 + y));
    if (typeGrid_[y][x] != type) {
        // not seen lock (a universe entering mid-game): a seed of its own
        seedGrid_[y][x] = hash32(0xC0FFEEull + (uint64_t)(x * 977 + y * 31) + ((uint64_t)spawnN_ << 20));
        typeGrid_[y][x] = (int8_t)type;
    }
    return seedGrid_[y][x];
}

// =====================================================================================================
// The collection
// =====================================================================================================

// A book's place: the next free one along the runs (a shelf: standing side by side; a row of piles on the floor:
// lying, three to five a pile, each pile a little askew). Its size and look come from its seed.
bool LibraryUniverse::place(Group& g) {
    Rng r(0xB00Cull ^ ((uint64_t)g.seed * 2654435761ull));
    const float h = r.range(0.58f, 0.84f), t = r.range(0.23f, 0.37f), d = r.range(0.4f, 0.5f);
    g.style = r.irange(0, 4);
    g.tone = r.range(0.9f, 1.06f);
    const float askew = r.range(-0.18f, 0.18f);
    while (runIdx_ < (int)runs_.size()) {
        const Run& R = runs_[runIdx_];
        if (runOff_ == 0.f && pileN_ == 0 && std::getenv("REFALL_LIBRARY_RUNS")) // (a test hook: where each run starts)
            std::fprintf(stderr, "[library] run %d (kind %d, feature %d) starts with book %zu (line %.1f)\n", runIdx_, R.kind, R.feat,
                         groups_.size(), groups_.size() / (BOOKS_LINE * paceScale()));
        if (R.feat >= 0) featUnlocked_[R.feat] = true;
        if (R.kind == 0) {
            if (runOff_ + t <= R.len) {
                const float hh = std::min(h, R.maxH);
                g.lying = false;
                g.hx = t * 0.5f - 0.005f;
                g.hy = hh * 0.5f;
                g.hd = d * 0.5f;
                g.along = R.along;
                g.out = R.out;
                g.level = R.level;
                g.c = R.start + R.along * (runOff_ + t * 0.5f) + UP * (hh * 0.5f) - R.out * (d * 0.5f + 0.05f);
                runOff_ += t + 0.01f;
                return true;
            }
        } else {
            const float spacing = 1.f;
            const int pileMax = 3 + (int)(hash32((uint64_t)(runIdx_ * 31 + pileN_)) % 3);
            if ((int)runOff_ >= pileMax) { pileN_++; runOff_ = 0.f; pileH_ = 0.f; }
            if ((pileN_ + 0.5f) * spacing <= R.len) {
                const float a = askew * 0.6f + 0.25f * ((hash32((uint64_t)(runIdx_ * 7 + pileN_)) & 255) / 255.f - 0.5f);
                const vec3 al = R.along * std::cos(a) + R.out * std::sin(a);
                g.lying = true;
                g.hx = h * 0.5f;
                g.hy = t * 0.36f; // (lying, a book reads thinner)
                g.hd = d * 0.5f;
                g.along = al;
                g.out = cross(al, UP) * (dot(cross(al, UP), R.out) > 0.f ? 1.f : -1.f);
                g.level = -1;
                const vec3 base = R.start + R.along * ((pileN_ + 0.5f) * spacing) - R.out * 0.35f;
                g.c = base + UP * (pileH_ + g.hy);
                pileH_ += 2.f * g.hy + 0.004f;
                runOff_ += 1.f;
                return true;
            }
        }
        runIdx_++;
        runOff_ = 0.f;
        pileN_ = 0;
        pileH_ = 0.f;
    }
    return false;
}

bool LibraryUniverse::shelve(int type, uint32_t seed) {
    Group g;
    g.type = type;
    g.seed = seed;
    if (!place(g)) return false;
    g.t0 = worldTime_;
    groups_.push_back(g);
    return true;
}

// A book as an instance: centered at c in its frame (along / up / out), k its size, grown from its foot by `grow`.
// The shelved books keep to the pieces' colors, muted toward the walls so the board stays the brightest thing.
void LibraryUniverse::pushBook(std::vector<Inst>& out, const Group& g, const vec3& c, const vec3& along, const vec3& outD, float k,
                               float grow, float glow, int index, float vivid) {
    if (grow <= 0.001f) return;
    const float hx = g.hx * k * grow, hy = g.hy * k * grow, hd = g.hd * k * grow;
    const vec3 foot = c - UP * (g.hy * k);
    const vec3 cc = foot + UP * hy;
    // standing: X across its thickness (along the shelf), Y up its length; lying: X up its thickness, Y along its length
    const vec3 X = g.lying ? UP * hy : along * hx;
    const vec3 Y = g.lying ? along * hx : UP * hy;
    const vec3 Z = outD * hd;
    // (in flight, vivid: its piece's full color)
    const vec3 col = lerp(lerp(pieceColor(g.type), pal_.wall, 0.38f) * (0.9f * g.tone), pieceColor(g.type), vivid);
    Inst in;
    in.c[0] = cc.x; in.c[1] = cc.y; in.c[2] = cc.z; in.c[3] = (float)M_BOOK;
    in.x[0] = X.x; in.x[1] = X.y; in.x[2] = X.z; in.x[3] = (float)g.style;
    in.y[0] = Y.x; in.y[1] = Y.y; in.y[2] = Y.z; in.y[3] = glow;
    in.z[0] = Z.x; in.z[1] = Z.y; in.z[2] = Z.z; in.z[3] = 0.f;
    in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = (float)(g.seed % 1000) / 97.f;
    in.e[0] = -100.f; in.e[1] = -1.f; in.e[2] = (float)g.level; in.e[3] = (float)index;
    out.push_back(in);
}

void LibraryUniverse::fly(int type, uint32_t seed, const vec3& board, float delay) {
    if (!shelve(type, seed)) return;
    groups_.back().t0 = 1e9f; // (not in its place until it lands)
    Flight fl;
    fl.group = (int)groups_.size() - 1;
    fl.board = board;
    fl.t = -delay;
    fl.spin = ((seed >> 8) & 1) ? 1.f : -1.f;
    flights_.push_back(fl);
}

int LibraryUniverse::booksFor() {
    const float before = bookAcc_;
    bookAcc_ += BOOKS_LINE * paceScale();
    return std::clamp((int)std::floor(bookAcc_ + 1e-4f) - (int)std::floor(before + 1e-4f), 0, Game::W);
}

void LibraryUniverse::resetGrowth() {
    groups_.clear();
    bookAcc_ = 0.f;
    flights_.clear();
    runIdx_ = 0;
    runOff_ = 0.f;
    pileN_ = 0;
    pileH_ = 0.f;
    for (int i = 0; i < F_COUNT_; i++) featUnlocked_[i] = false;
    shelfInst_.clear();
    settled_ = 0;
    shelfDirty_ = true;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void LibraryUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Spawn: spawnN_++; break;
    case GameEvent::Lock: {
        int i = 0;
        for (auto& c : ev.cells) {
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) {
                seedGrid_[c.y][c.x] = hash32(((uint64_t)spawnN_ << 3) + (uint64_t)i);
                typeGrid_[c.y][c.x] = (int8_t)c.type;
            }
            i++;
        }
        break;
    }
    case GameEvent::Clear: {
        if (ev.count >= 4) quadT_ = 0.f;
        std::vector<GameEvent::CellInfo> cs;
        for (auto& c : ev.cells)
            if (c.x >= 0 && c.x < Game::W && c.y >= 0 && c.y < Game::H) cs.push_back(c);
        // rows bottom first, each left to right: the order they go on the shelves
        std::sort(cs.begin(), cs.end(), [](const GameEvent::CellInfo& a, const GameEvent::CellInfo& b) { return a.y != b.y ? a.y > b.y : a.x < b.x; });
        bool full[Game::H] = {};
        // each row's share of books (the song's pace), and the gap between their launches: a quad's dozen spread
        // over two seconds; a bigger batch (a short song's quad: up to thirty) closer together, within 2.5 s
        int rowBooks[Game::H] = {}, total = 0;
        for (auto& c : cs)
            if (!full[c.y]) { full[c.y] = true; rowBooks[c.y] = booksFor(); total += rowBooks[c.y]; }
        for (bool& b : full) b = false;
        const float gap = std::min(0.16f, 2.5f / (float)std::max(1, total));
        int i = 0;
        for (auto& c : cs) {
            const int type = std::clamp(c.type, 0, 6);
            const uint32_t sd = cellSeed(c.x, c.y, type);
            full[c.y] = true;
            const vec3 bp = layout::cellPos((float)c.x, (float)c.y);
            if (c.y >= Game::HIDDEN) fx_.push_back({type, sd, bp, 0.f});
            // each line's books leave from some of its cells, in its pieces' colors
            if (bookCell(c.x, rowBooks[c.y])) fly(type, sd, bp, 0.1f + gap * i++);
        }
        // the seeds follow the rows down
        int dst = Game::H - 1;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (full[y]) continue;
            if (dst != y) {
                std::memcpy(seedGrid_[dst], seedGrid_[y], sizeof(seedGrid_[y]));
                std::memcpy(typeGrid_[dst], typeGrid_[y], sizeof(typeGrid_[y]));
            }
            dst--;
        }
        for (; dst >= 0; dst--) std::memset(typeGrid_[dst], -1, sizeof(typeGrid_[dst]));
        break;
    }
    case GameEvent::TopOut: {
        // the stacks topple out of the bottom of the well
        overT_ = 0.f;
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            Tumble t;
            t.type = std::clamp(c.type, 0, 6);
            t.seed = cellSeed(c.x, c.y, t.type);
            t.p = layout::cellPos((float)c.x, (float)c.y);
            t.delay = 0.05f * (Game::H - 1 - c.y) + 0.15f * (hash32((uint64_t)(c.x * 31 + c.y)) & 255) / 255.f;
            t.v = vec3(rng_.range(-1.2f, 1.2f), rng_.range(-1.f, 1.f), rng_.range(-2.f, -0.5f));
            t.rotV = rng_.range(-3.f, 3.f);
            t.t = 0.f;
            tumbles_.push_back(t);
        }
        (void)top;
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The board
// =====================================================================================================

void LibraryUniverse::buildBoard(const Game& g) {
    const vec3 X(1.f, 0.f, 0.f), Z(0.f, 0.f, 1.f);
    stLevel_ = -1.f;
    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            pushCell(cells_, c.type, p, X, UP, 1.f, 0.f, cellSeed(x, y, c.type), y > 0 && g.cell(x, y - 1).type >= 0);
        }
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        for (int i = 0; i < 4; i++)
            pushCell(cells_, p.type, layout::cellPos(cells[i][0] + dx, cells[i][1] + dy), X, UP, 1.f, 0.f,
                     hash32(((uint64_t)spawnN_ << 3) + (uint64_t)i), inPiece(cells[i][0], cells[i][1] - 1));
        // the ghost: a fine outline where it lands
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
            const vec3 col = lerp(pieceColor(p.type), vec3(1.f), 0.2f);
            const float wpx = std::max(1.5f, 2.6f * height_ / 1080.f);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                const float h = 0.47f, z = 0.01f;
                auto L = [&](const vec3& a, const vec3& b) {
                    cellLines_.push_back({{a.x, a.y, a.z, wpx}, {b.x, b.y, b.z, 0.85f}, {col.x, col.y, col.z, 0.f}});
                };
                if (!has(c[0], c[1] - 1)) L(vec3(w.x - h, w.y + h, z), vec3(w.x + h, w.y + h, z));
                if (!has(c[0], c[1] + 1)) L(vec3(w.x - h, w.y - h, z), vec3(w.x + h, w.y - h, z));
                if (!has(c[0] - 1, c[1])) L(vec3(w.x - h, w.y - h, z), vec3(w.x - h, w.y + h, z));
                if (!has(c[0] + 1, c[1])) L(vec3(w.x + h, w.y - h, z), vec3(w.x + h, w.y + h, z));
            }
        }
    }
    // the previews: a next piece in the books it will fall with (the seeds of the spawn to come), the held one in its
    // type's own; the cells a little apart (a small piece reads as its books)
    auto preview = [&](int type, vec2 center, float sc, uint64_t spawn) {
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
            pushCell(cells_, type, vec3(center.x + ox * sc, center.y - oy * sc, 0.f), X, UP, sc * 0.95f, 0.f,
                     hash32((spawn << 3) + (uint64_t)i));
        }
    };
    if (!g.over()) {
        preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), 0x40D0ull + (uint64_t)g.holdType());
        for (int i = 0; i < 3; i++) {
            const vec3 c = layout::nextCenter(i);
            if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), (uint64_t)spawnN_ + 1 + (uint64_t)i);
        }
    }
    stLevel_ = 0.f;
}

// =====================================================================================================
// The HUD
// =====================================================================================================

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

// The HUD's plates (the lettering: hudLine, the host's).
void LibraryUniverse::drawHud(const Game& g, const std::vector<HudRect>& panels) {
    const vec3 halo = pal_.halo;
    const vec3 accent = pal_.accent;
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
}

// The lettering, by the host (text.hpp): a clean grotesque, Instrument Sans, charcoal (or warm white at night) on a
// soft halo, the variant's one accent for the labels. The corner lines (the song, its section, the universe) sit on
// little rounded tags in the card's color, so they read over the busy shelves.
void LibraryUniverse::hudLine(const HudText& h, text::Line& out) {
    const vec3 ink = pal_.ink, halo = pal_.halo, accent = pal_.accent;
    text::Font* reg = text::font(FONT_REG);
    text::Font* semi = text::font(FONT_SEMI);
    text::Font* ital = text::font(FONT_ITAL);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    float haloK = 1.f;
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: put(semi, text::upper(h.text), base * 0.66f, base * 0.16f, ink, saturate(0.3f + h.alpha * 1.5f) * 0.85f, base * 0.06f); break;
    case HUD_SECTION: put(semi, text::upper(h.text), base * 0.72f, base * 0.22f, accent, 0.3f + h.alpha * 1.7f); break;
    case HUD_TITLE: put(ital, h.text, base * 1.06f, 0.f, ink, h.alpha * 1.5f, -base * 0.08f); break;
    case HUD_INFO: put(reg, text::upper(h.text), base * 0.62f, base * 0.14f, ink, saturate(alpha * 1.5f) * 0.85f); break;
    case HUD_LABEL: put(semi, text::upper(h.text), base * 0.68f, base * 0.24f, accent, saturate(alpha * 1.3f) * 0.95f); break;
    case HUD_VALUE: put(reg, h.text, base * 1.02f, base * 0.01f, ink, alpha * 0.96f, -base * 0.08f); break;
    case HUD_NOTICE: put(semi, h.text, base * 0.9f, base * 0.02f, accent, alpha); break;
    case HUD_KEY: put(semi, h.text, base * 0.7f, base * 0.08f, ink, alpha); break;
    case HUD_TOAST: put(semi, text::upper(h.text), base * 0.86f, base * 0.24f, ink, alpha, -base * 0.1f); haloK = 0.65f; break;
    case HUD_TOAST_SUB: put(reg, h.text, base * 0.95f, base * 0.02f, accent, alpha * 0.95f); haloK = 0.65f; break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * height_ / 1080.f) put(semi, text::upper(h.text), base * 0.8f, base * 0.22f, ink, alpha);
        else put(reg, h.text, base * 0.92f, base * 0.01f, h.accent ? accent : ink, alpha);
        haloK = 0.75f;
        break;
    default: put(reg, h.text, base * 0.7f, base * 0.04f, h.accent ? accent : ink, alpha); break;
    }
    // the letters a touch bold, on their halo
    out.look.weight = 0.0075f;
    out.look.halo = halo;
    out.look.haloA = haloK;
    out.look.haloW = 0.095f;
    out.look.haloSmooth = true;
    // the corner lines' tags: a pill in the card's color
    if (h.role == HUD_TITLE || h.role == HUD_INFO || h.role == HUD_UNIVERSE || h.role == HUD_SECTION) {
        out.plate.color = ugl::toSrgb(pal_.card);
        out.plate.alpha = std::min(1.f, alpha * 1.6f) * 0.92f;
        out.plate.padX = 0.585f;
        out.plate.padY = 0.45f;
        out.plate.radius = 0.54f;
    }
    out.alpha = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
}

// =====================================================================================================
// The camera
// =====================================================================================================

// The diorama's view: from above and far, through a long lens (a tabletop model), the whole room framed around the
// well, drifting very slowly.
float LibraryUniverse::lampOn(int i) const {
    const float k = std::fmod((float)i * 0.618034f, 1.f); // each lamp its turn
    return smoothstepf(0.15f + 0.6f * k, 0.25f + 0.6f * k, setup_) * (1.f - smoothstepf(0.6f * k, 0.1f + 0.6f * k, teardown_));
}

void LibraryUniverse::updateCamera(float dt) {
    (void)dt;
    tanW_ = std::tan(13.f * PI / 180.f);
    const float sway = 0.4f * std::sin(worldTime_ * 0.045f), bob = 0.12f * std::sin(worldTime_ * 0.063f);
    // around a song the camera comes in to the room from further back, and draws back there
    const float away = std::max(1.f - smoothstepf(0.f, 0.9f, setup_), smoothstepf(0.f, 1.f, teardown_));
    targetW_ = vec3(0.f, 2.3f, -1.0f);
    eyeW_ = targetW_ + vec3(sway, 11.2f + bob, 25.f) * (1.f + 0.45f * away);
    fwdW_ = normalize(targetW_ - eyeW_);
    rightW_ = normalize(cross(fwdW_, UP));
    upW_ = cross(rightW_, fwdW_);
    camD_ = length(targetW_ - eyeW_);
    const mat4 view = mat4::lookAt(vec3(0.f), fwdW_, UP);
    vpW_ = mat4::perspective(2.f * std::atan(tanW_), aspect_, 0.1f, 200.f) * view;
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
    bare_ = f.bare;
    // REFALL_LIBRARY_TIMING=1: the frame's cost (GPU included: glFinish around it), every 200 frames
    static const bool timing = std::getenv("REFALL_LIBRARY_TIMING") != nullptr;
    struct Timer {
        bool on;
        double t0 = 0;
        const LibraryUniverse* u;
        Timer(bool o, const LibraryUniverse* uu) : on(o), u(uu) {
            if (on) { glFinish(); t0 = now(); }
        }
        static double now() {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        ~Timer() {
            if (!on) return;
            glFinish();
            static double acc = 0, mx = 0;
            static int n = 0;
            const double d = now() - t0;
            acc += d;
            mx = std::max(mx, d);
            if (++n == 200) {
                std::fprintf(stderr, "[library] frame %.2f ms (max %.2f) at %d lines\n", acc / n, mx, u->lines_);
                acc = 0; mx = 0; n = 0;
            }
        }
    } timer(timing, this);
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.0f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) { // (a break's blank board is never over)
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    time_ += dt;
    worldTime_ += wdt;
    setup_ = f.setup;
    teardown_ = f.teardown;

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
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.2f : 0.5f, dt);
    quadT_ += dt;
    kickK_ = std::max(kickK_ * std::exp(-dt * 5.f), kickIn_);
    kickIn_ = 0.f;
    refreshK_ = approach(refreshK_, 0.f, 0.5f, dt);
    beatPulse_ = music.hasSong ? std::pow(1.f - (float)(music.beat - std::floor(music.beat)), 3.f) : 0.f;

    // ---- The game's lines -> the collection (a new game empties the shelves).
    {
        const int hookStart = startLines(), hookRate = rateLines(time_);
        const int lines = f.bare ? lines_ : linesHere(game) + hookStart + hookRate; // (a break: the shelves as they were)
        if (linesSeen_ >= 0 && lines < lines_ && !game.over()) resetGrowth();
        lines_ = lines;
        linesSeen_ = lines;
        if (runs_.empty()) buildRuns();
        // the test hooks' lines: the static offset's books put straight in their places (screenshots), the grow
        // rate's lines shelved like real ones (their books flying to their places)
        if (!hookPlaced_) {
            hookPlaced_ = true;
            Rng hr(0xB00C5ull);
            int books = 0;
            for (int l = 0; l < hookStart; l++) books += booksFor();
            for (int l = 0; l < books; l++)
                if (shelve(hr.irange(0, 6), (uint32_t)hr.next())) groups_.back().t0 = -100.f;
            for (int k = 0; k < F_COUNT_; k++)
                if (featUnlocked_[k]) featGrow_[k] = 1.f;
        }
        while (hookLines_ < hookRate) {
            hookLines_++;
            const int k = booksFor();
            for (int x = 0, j = 0; x < Game::W; x++)
                if (bookCell(x, k)) fly(rng_.irange(0, 6), (uint32_t)rng_.next(), layout::cellPos((float)x, (float)(Game::H - 1)), 0.1f + 0.16f * j++);
        }
        grow_ = (float)lines_;
    }
    if (worldDirty_) {
        buildWorld();
        for (int m = 0; m < MESH_COUNT; m++) {
            glBindBuffer(GL_ARRAY_BUFFER, statVbo_[m]);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(stat_[m].size() * sizeof(Inst)), stat_[m].empty() ? nullptr : stat_[m].data(), GL_STATIC_DRAW);
            statCount_[m] = stat_[m].size();
        }
        glBindBuffer(GL_ARRAY_BUFFER, glassVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(glass_.size() * sizeof(Inst)), glass_.empty() ? nullptr : glass_.data(), GL_STATIC_DRAW);
    }
    updateCamera(wdt);
    // the features (grown in from the floor when first needed), the lights: the room's, the mezzanine's, the low
    // bookcase's
    for (int k = 0; k < F_COUNT_; k++) featGrow_[k] = approach(featGrow_[k], featUnlocked_[k] ? 1.f : 0.f, 0.8f, dt);
    levelOn_[0] = 1.f;
    levelOn_[1] = smoothstepf(0.5f, 1.f, featGrow_[F_MEZZ]);
    levelOn_[2] = smoothstepf(0.5f, 1.f, featGrow_[F_LOW]);

    // ---- Dynamic instances: the collection's books.
    for (auto& v : dyn_) v.clear();
    cells_.clear();
    fxInst_.clear();
    cellLines_.clear();
    sprites_.clear();
    const int nG = (int)groups_.size();
    auto pushGroup = [&](std::vector<Inst>& out, int i, float grow, float glow) {
        const Group& g = groups_[i];
        pushBook(out, g, g.c, g.along, g.out, 1.f, grow, glow, i);
    };
    // settled books (grown, their light faded) go to the kept buffer, the others are drawn as they settle
    while (settled_ < groups_.size() && worldTime_ - groups_[settled_].t0 > 3.f) {
        pushGroup(shelfInst_, (int)settled_, 1.f, 0.f);
        settled_++;
        shelfDirty_ = true;
    }
    if (shelfDirty_) {
        glBindBuffer(GL_ARRAY_BUFFER, shelfVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(shelfInst_.size() * sizeof(Inst)), shelfInst_.empty() ? nullptr : shelfInst_.data(),
                     GL_STATIC_DRAW);
        shelfDirty_ = false;
    }
    for (int i = (int)settled_; i < nG; i++) {
        const float a = worldTime_ - groups_[i].t0;
        if (a < 0.f) continue;
        // settling into its place, a soft light fading
        pushGroup(dyn_[MESH_BOX], i, a > 0.6f ? 1.f : easeOutBack(a / 0.5f), 0.5f * std::exp(-std::max(0.f, a - 0.6f) * 1.4f));
    }

    // ---- Flights: a cleared line's books, from behind their cells' places in the well out into the open room and
    // into their places. A slow, legible arc: out from behind the well, a glide through the room in front of their
    // shelf, then in; bigger than life on the way, in their piece's full color with a soft glow and a short ribbon of
    // light behind them, taking on their shelf tint as they land. (The room is drawn under the board and the HUD, so
    // nothing of this ever passes in front of them.)
    auto flightAt = [&](const Flight& fl, const Group& g, float s) {
        const float e = s * s * (3.f - 2.f * s);
        const float u = 1.f - e;
        return fl.p0 * (u * u * u) + fl.c1 * (3.f * u * u * e) + fl.c2 * (3.f * u * e * e) + g.c * (e * e * e);
    };
    for (auto it = flights_.begin(); it != flights_.end();) {
        Flight& fl = *it;
        fl.t += wdt;
        if (fl.group < 0 || fl.group >= (int)groups_.size()) { it = flights_.erase(it); continue; }
        Group& g = groups_[fl.group];
        if (fl.t < 0.f) { ++it; continue; }
        const vec3 oT = g.c;
        if (!fl.launched) {
            fl.launched = true;
            // the cell's pixel and size through the board's camera, the world ray through it
            const vec4 c0 = vpB_ * vec4(fl.board, 1.f), c1 = vpB_ * vec4(fl.board + vec3(0.f, 0.5f, 0.f), 1.f);
            const vec2 ndc{c0.x / c0.w, c0.y / c0.w};
            const float halfNdc = std::fabs(c1.y / c1.w - ndc.y);
            const vec3 dir = normalize(fwdW_ + rightW_ * (ndc.x * tanW_ * aspect_) + upW_ * (ndc.y * tanW_));
            const float dT = std::max(1.f, dot(oT - eyeW_, fwdW_));
            const float d0 = std::clamp(dT * 0.8f, std::min(2.f, dT), dT);
            fl.p0 = eyeW_ + dir * (d0 / std::max(0.2f, dot(dir, fwdW_)));
            fl.k0 = 2.f * halfNdc * d0 * tanW_ / std::max(0.05f, 2.f * g.hy);
            fl.out0 = normalize(vec3(-fwdW_.x, 0.f, -fwdW_.z));
            // the glide: a point out in the room in front of its place (on the open side of the screen, past the
            // HUD's columns), reached through a rise beside the well
            // (each its own height, so a quad's books don't bunch)
            const float hv = ((g.seed >> 4) & 255) / 255.f;
            const vec3 front = oT + g.out * (1.6f + 0.6f * hv) + UP * (0.6f + 0.7f * hv);
            fl.c2 = front;
            fl.c1 = lerp(fl.p0, front, 0.55f) + UP * (1.1f + 1.0f * hv);
            fl.dur = 2.0f + 0.15f * std::log2(1.f + length(oT - fl.p0) / 4.f);
        }
        const float s = saturate(fl.t / fl.dur);
        if (s >= 1.f) {
            g.t0 = worldTime_ - 0.6f; // in its place, full size, its light fading from now
            g.landT = worldTime_;
            it = flights_.erase(it);
            continue;
        }
        if (!f.bare) {
            const float e = s * s * (3.f - 2.f * s);
            const vec3 p = flightAt(fl, g, s);
            // its spine turns from us toward its place, once round on the way
            const float turn = fl.spin * TAU * (1.f - smoothstepf(0.f, 0.8f, s));
            const vec3 o0 = normalize(lerp(fl.out0, g.out, smoothstepf(0.f, 0.8f, s)));
            const vec3 a0 = normalize(cross(UP, o0));
            const vec3 ou = o0 * std::cos(turn) + a0 * std::sin(turn);
            vec3 al = normalize(cross(UP, ou));
            if (dot(cross(UP, g.out), g.along) < 0.f) al = al * -1.f;
            const float big = 1.f + 0.7f * std::sin(PI * std::min(1.f, e * 1.15f));
            const float land = smoothstepf(0.8f, 1.f, s);
            pushBook(dyn_[MESH_BOX], g, p, al, ou, lerpf(fl.k0, 1.f, e) * big, 1.f, 0.3f * (1.f - land) + 0.15f, fl.group, 1.f - land);
            // the ribbon of light behind it
            const vec3 pc = pieceColor(g.type);
            const vec3 rc = lerp(pc, vec3(1.f), 0.35f);
            for (int k = 1; k <= 12; k++) {
                const float sk = s - k * 0.018f;
                if (sk <= 0.02f) break;
                const vec3 q = flightAt(fl, g, sk);
                const float fade = (1.f - k / 13.f) * (1.f - land);
                sprites_.push_back({{q.x, q.y, q.z, 0.2f * (1.f - 0.5f * k / 12.f)}, {rc.x, rc.y, rc.z, 0.9f * fade * fade}});
            }
            sprites_.push_back({{p.x, p.y, p.z, 0.55f}, {rc.x, rc.y, rc.z, 0.5f * (1.f - land)}});
        }
        ++it;
    }
    // ---- a soft pulse of light where each book lands, so you see where it went
    for (int i = std::max(0, nG - 64); i < nG; i++) {
        const Group& g = groups_[i];
        const float a = worldTime_ - g.landT;
        if (a < 0.f || a > 1.f) continue;
        const vec3 rc = lerp(pieceColor(g.type), vec3(1.f), 0.4f);
        const float k = 1.f - a;
        const vec3 p = g.c + g.out * 0.25f;
        sprites_.push_back({{p.x, p.y, p.z, 0.4f + 0.6f * a}, {rc.x, rc.y, rc.z, 1.4f * k * k}});
    }

    // ---- The drop: loose pages fluttering up from the bookcases, behind the well
    if (!f.bare && dropAmt_ > 0.01f) {
        const vec3 paper = pal_.paper * 0.95f;
        for (int i = 0; i < 18; i++) {
            const float hs = (hash32((uint64_t)i * 977u) & 1023) / 1023.f, hs2 = (hash32((uint64_t)i * 131u + 7u) & 1023) / 1023.f;
            const float ph = std::fmod(worldTime_ * (0.07f + 0.03f * hs) + hs2, 1.f);
            const int side = (i % 2) ? 1 : -1;
            const WallFrame w = wallAt(side, 0.45f + 0.4f * hs);
            const vec3 p0 = w.p + w.out * (CASE_D + 0.3f) + UP * (0.6f + 2.0f * hs2);
            const vec3 p = p0 + w.out * (ph * 2.4f) + UP * (ph * 2.8f + 0.2f * std::sin(ph * 11.f + i)) + w.along * (0.7f * std::sin(ph * 4.f + i));
            const float a = dropAmt_ * std::sin(PI * ph);
            if (a < 0.02f) continue;
            const float th = ph * 9.f + i, ph2 = std::sin(ph * 7.f + i * 1.7f) * 1.1f;
            const vec3 ax = vec3(std::cos(th), 0.f, std::sin(th));
            const vec3 ay = vec3(-std::sin(th) * std::sin(ph2), std::cos(ph2), std::cos(th) * std::sin(ph2));
            const vec3 az = normalize(cross(ax, ay));
            const vec3 X = ax * (0.13f * a), Y = ay * (0.17f * a), Z = az * 0.004f;
            Inst in;
            in.c[0] = p.x; in.c[1] = p.y; in.c[2] = p.z; in.c[3] = (float)M_PLASTER;
            in.x[0] = X.x; in.x[1] = X.y; in.x[2] = X.z; in.x[3] = 0.f;
            in.y[0] = Y.x; in.y[1] = Y.y; in.y[2] = Y.z; in.y[3] = 0.f;
            in.z[0] = Z.x; in.z[1] = Z.y; in.z[2] = Z.z; in.z[3] = 0.f;
            in.col[0] = paper.x; in.col[1] = paper.y; in.col[2] = paper.z; in.col[3] = 0.f;
            in.e[0] = -100.f; in.e[1] = -1.f; in.e[2] = -1.f; in.e[3] = -10.f;
            dyn_[MESH_BOX].push_back(in);
        }
    }

    // ---- A cleared row: its stacks swell and dissolve into light where they stand.
    for (auto it = fx_.begin(); it != fx_.end();) {
        it->t += dt;
        if (it->t > FX_T) { it = fx_.erase(it); continue; }
        if (!f.bare && it->t > 0.f) {
            const float s = saturate(it->t / FX_T);
            pushCell(fxInst_, it->type, it->p, vec3(1, 0, 0), UP, 1.f, std::max(0.02f, s), it->seed);
        } else if (!f.bare) {
            pushCell(cells_, it->type, it->p, vec3(1, 0, 0), UP, 1.f, 0.f, it->seed);
        }
        ++it;
    }
    // ---- Game over: the stacks topple out of the bottom of the well, away from us.
    for (auto it = tumbles_.begin(); it != tumbles_.end();) {
        Tumble& t = *it;
        t.t += dt;
        const float a = t.t - t.delay;
        if (a > 3.f || overT_ < 0.f) { it = tumbles_.erase(it); continue; }
        if (!f.bare) {
            const float tt = std::max(0.f, a);
            const vec3 p = t.p + vec3(t.v.x * tt, t.v.y * tt - 7.f * tt * tt, t.v.z * tt);
            const float r = t.rotV * tt;
            const vec3 al(std::cos(r), std::sin(r), 0.f), up(-std::sin(r), std::cos(r), 0.f);
            pushCell(cells_, t.type, p, al, up, 1.f, 0.f, t.seed);
        }
        ++it;
    }
    if (!f.bare) buildBoard(game);

    // ---- Lights: the downlights of the floors in use, the 16 that matter most.
    const float lampLevel = pal_.ledK * (0.85f + 0.08f * wm + 0.15f * wp + 0.2f * kickK_ * wp + 0.25f * dropAmt_ * (0.6f + 0.4f * beatPulse_)) *
                            (1.f - 0.3f * slow_);
    std::vector<vec4> LP, LC;
    {
        std::vector<std::pair<float, int>> cand;
        for (int i = 0; i < (int)lamps_.size(); i++) {
            const Lamp& l = lamps_[i];
            const float on = levelOn_[std::clamp(l.level, 0, 7)] * lampOn(i);
            if (on <= 0.01f) continue;
            if (l.r > 0.f) sprites_.push_back({{l.p.x, l.p.y, l.p.z, l.r}, {l.col.x, l.col.y, l.col.z, l.k * lampLevel * on}});
            const float d = length(l.p - targetW_);
            cand.push_back({on / (1.f + d / std::max(2.f, camD_ * 0.4f)), i});
        }
        cand.erase(std::remove_if(cand.begin(), cand.end(), [&](const std::pair<float, int>& c) { return lamps_[c.second].range <= 0.f; }), cand.end());
        std::sort(cand.begin(), cand.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
        for (int j = 0; j < (int)cand.size() && j < 16; j++) {
            const Lamp& l = lamps_[cand[j].second];
            LP.push_back(vec4(l.p, l.range));
            LC.push_back(vec4(l.col, l.k * lampLevel * levelOn_[std::clamp(l.level, 0, 7)] * lampOn(cand[j].second) * 0.9f));
        }
    }
    // dust in the daylight (barely any at night); it hangs still in slow motion
    {
        const float calm = (1.f - 0.6f * wp) * (pal_.light ? 0.5f : 0.25f);
        const vec3 size(camD_ * 1.1f, camD_ * 0.6f, camD_ * 0.6f);
        const vec3 org = targetW_ - vec3(size.x * 0.5f, size.y * 0.45f, -0.3f);
        const vec3 mc = lerp(pal_.key / std::max(0.01f, pal_.keyK), vec3(1.f), 0.5f);
        for (const Mote& m : motes_) {
            vec3 q = m.p + vec3(std::sin(worldTime_ * 0.05f + m.seed * 30.f) * 0.03f, std::fmod(worldTime_ * 0.005f * (0.5f + m.seed) + 10.f, 1.f),
                                std::cos(worldTime_ * 0.04f + m.seed * 17.f) * 0.03f);
            q.y = q.y - std::floor(q.y);
            const vec3 p = org + vec3(q.x * size.x, q.y * size.y, q.z * size.z);
            const float tw = 0.5f + 0.5f * std::sin(worldTime_ * (1.f + m.seed * 2.f) + m.seed * 40.f);
            sprites_.push_back({{p.x, p.y, p.z, camD_ * 0.0018f}, {mc.x, mc.y, mc.z, calm * (0.2f + 0.5f * tw)}});
        }
    }

    // ---- Uniforms shared by the passes.
    const Palette& P = pal_;
    const vec3 keyDir = normalize(VARIANTS[variant_].keyDir);
    const float keyK = (1.f + 0.06f * wp + 0.08f * dropAmt_) * (1.f - 0.55f * slow_);
    const vec3 slowTint(0.72f, 0.84f, 1.18f);
    const vec3 fogCol = P.light ? lerp(P.skyA, vec3(1.f), 0.55f) : lerp(P.skyA, P.skyB, 0.6f) * 0.5f;
    // the collection's light: the drop runs it slowly and on, a quad once along it
    float dropT = -10.f, dropK = 0.f;
    if (quadT_ < 3.f) { dropT = quadT_ / 2.4f * 1.3f - 0.15f; dropK = 0.28f; }
    else if (dropAmt_ > 0.01f) { dropT = std::fmod(worldTime_ * 0.07f, 1.4f) - 0.2f; dropK = 0.2f * dropAmt_ * (0.75f + 0.25f * beatPulse_); }
    auto lighting = [&](GLuint p, bool board) {
        if (board) {
            // the blocks: a clean white key (their faces read as their color), a cool shade on the sides away from it
            set3f(p, "uAmbSky", lerp(P.ambSky / std::max(0.05f, VARIANTS[variant_].ambK), vec3(0.75f), 0.35f) * 0.42f);
            set3f(p, "uAmbGround", lerp(P.ambGround / std::max(0.05f, VARIANTS[variant_].ambK), vec3(0.6f), 0.35f) * 0.3f);
            set3f(p, "uKeyDir", normalize(vec3(-0.45f, 0.72f, 0.6f)));
            set3f(p, "uKeyCol", vec3(1.f, 0.98f, 0.95f) * (0.68f + 0.06f * wp + 0.1f * dropAmt_) * lerp(vec3(1.f), slowTint, slow_));
            set1i(p, "uNL", 0);
            set1f(p, "uShadowOn", 0.f);
            set1f(p, "uFogD", 0.f);
            set1f(p, "uFogStart", 1e6f);
            set3f(p, "uFogCol", fogCol);
        } else {
            set3f(p, "uAmbSky", P.ambSky * (1.f - 0.35f * slow_) * lerp(vec3(1.f), slowTint, slow_));
            set3f(p, "uAmbGround", P.ambGround);
            set3f(p, "uKeyDir", keyDir);
            set3f(p, "uKeyCol", P.key * keyK * lerp(vec3(1.f), slowTint, slow_));
            const int n = (int)LP.size();
            set1i(p, "uNL", n);
            if (n) {
                glUniform4fv(ugl::U(p, "uLP"), n, &LP[0].x);
                glUniform4fv(ugl::U(p, "uLC"), n, &LC[0].x);
            }
            set1f(p, "uShadowOn", 1.f);
            set1f(p, "uFogD", 0.f);
            set1f(p, "uFogStart", 1e6f);
            set3f(p, "uFogCol", fogCol);
        }
    };
    auto common = [&](GLuint p, bool board) {
        set3f(p, "uEye", board ? eyeB_ : eyeW_);
        set1f(p, "uTime", worldTime_);
        setMat(p, "uVP", board ? vpB_ : vpW_);
        set3f(p, "uOrigin", board ? vec3(0.f) : eyeW_);
    };

    // ---- The drop's wave through the shelves: eased in and out with the drop, a pulse a beat (fuller on the bar's
    // downbeat and with the kicks); without a song, a slow pulse of its own
    float waveK = 0.f, beatPh = 0.f;
    if (dropAmt_ > 0.002f) {
        const double b = music.hasSong ? music.beat : worldTime_ * 2.0;
        beatPh = (float)(b - std::floor(b));
        const bool down = ((long long)std::floor(b) % 4 + 4) % 4 == 0;
        waveK = smoothstepf(0.f, 1.f, dropAmt_) * ((down ? 1.f : 0.68f) + 0.25f * kickK_) * (1.f - 0.6f * slow_);
    }
    // ---- The sun's shadow map, round what the camera sees.
    const vec3 shC(0.f, 2.f, -1.5f);
    const float shR = 11.f;
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
        set1f(progShadow_, "uWaveK", waveK);
        set1f(progShadow_, "uBeatPh", beatPh);
        set3f(progShadow_, "uOrigin", shC);
        glUniform1fv(ugl::U(progShadow_, "uFeat"), F_COUNT_, featGrow_);
        if (!shelfInst_.empty()) {
            glBindVertexArray(shelfVao_);
            glDrawElementsInstanced(GL_TRIANGLES, meshN_[MESH_BOX], GL_UNSIGNED_INT, nullptr, (GLsizei)shelfInst_.size());
        }
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

    // ---- The scene (multisampled): the atrium, its glass, the glows, the veil, then the well.
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(fogCol.x, fogCol.y, fogCol.z, 0.f);
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
    set1f(progSolid_, "uPremul", 0.f);
    const float skyK = 1.f;
    set3f(progSolid_, "uSkyA", lerp(P.skyA, P.skyA * slowTint * 0.45f, slow_) * skyK);
    set3f(progSolid_, "uSkyB", lerp(P.skyB, P.skyB * slowTint * 0.5f, slow_) * skyK);
    set3f(progSolid_, "uOutside", P.leaf);
    set3f(progSolid_, "uLed", P.led);
    set3f(progSolid_, "uPaper", P.paper);
    set3f(progSolid_, "uFloorB", P.floorB);
    set3f(progSolid_, "uLineCol", P.line);
    set3f(progSolid_, "uRimCol", P.rim);
    set3f(progSolid_, "uCloud", P.cloud);
    set3f(progSolid_, "uCloudShade", P.cloudShade);
    set1f(progSolid_, "uLineW", std::max(1.f, 1.6f * height_ / 1080.f));
    set1f(progSolid_, "uLampK", lampLevel);
    set1f(progSolid_, "uNight", P.night);
    set1f(progSolid_, "uWorld", 1.f);
    set1f(progSolid_, "uShelfLight", lampLevel);
    set1f(progSolid_, "uLevelH", SHELF_PITCH);
    glUniform1fv(ugl::U(progSolid_, "uFeat"), F_COUNT_, featGrow_);
    set1f(progSolid_, "uBeat", beatPulse_);
    glUniform1fv(ugl::U(progSolid_, "uLevelOn"), 8, levelOn_);
    set1f(progSolid_, "uDropT", dropT);
    set1f(progSolid_, "uDropK", dropK);
    set1f(progSolid_, "uDropSpan", 0.06f);
    set1f(progSolid_, "uWaveK", waveK);
    set1f(progSolid_, "uBeatPh", beatPh);
    set1f(progSolid_, "uGroupN", (float)std::max<size_t>(1, groups_.size()));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    set1i(progSolid_, "uShadow", 1);
    setMat(progSolid_, "uShadowVP", shVP);
    set3f(progSolid_, "uShadowO", shC);
    set1f(progSolid_, "uShadowTexel", 1.f / 2048.f);
    set1f(progSolid_, "uShadowBias", 2.f * shR / 2048.f * 4.f);
    glActiveTexture(GL_TEXTURE0);
    if (!shelfInst_.empty()) drawInst(shelfVao_, shelfVbo_, MESH_BOX, nullptr, shelfInst_.size());
    for (int m = 0; m < MESH_COUNT; m++) {
        if (statCount_[m]) {
            glBindVertexArray(statVao_[m]);
            glUniform1i(ugl::U(progSolid_, "uMesh"), m);
            glDrawElementsInstanced(GL_TRIANGLES, meshN_[m], GL_UNSIGNED_INT, nullptr, (GLsizei)statCount_[m]);
        }
        if (!dyn_[m].empty()) drawInst(dynVao_[m], dynVbo_[m], m, nullptr, dyn_[m].size());
    }
    // the balustrades' glass and the glows (no alpha: the frame's alpha is the stack's)
    glEnable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!glass_.empty()) {
        set1f(progSolid_, "uPremul", 1.f);
        drawInst(glassVao_, glassVbo_, MESH_BOX, nullptr, glass_.size());
        set1f(progSolid_, "uPremul", 0.f);
    }
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
        set1f(progSprite_, "uFogD", 0.5f / std::max(4.f, camD_ * 2.5f));
        set1f(progSprite_, "uFogStart", camD_ * 0.6f);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sprites_.size() * sizeof(Sprite)), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    // the frosted panel behind the well and the HUD's columns (none in a break)
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        // the well's ground: one deep saturated color, against which all seven pieces stand out; the HUD on cards
        set3f(progVeil_, "uVeil", P.veil);
        set3f(progVeil_, "uVeilTop", P.veilTop);
        set3f(progVeil_, "uCard", P.card);
        set3f(progVeil_, "uCardEdge", P.cardEdge);
        set1f(progVeil_, "uPx", 2.f * viewH_.y / height_);
        set1f(progVeil_, "uWellA", 1.f);
        set1f(progVeil_, "uColA", P.light ? 0.6f : 0.68f);
        // (the cards a little narrower than the columns, so more of the library shows)
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0] + 0.9f, layout::LEFT_COLUMN[1] + 0.3f, layout::LEFT_COLUMN[2] - 0.5f, layout::LEFT_COLUMN[3] - 0.5f);
        set4f(progVeil_, "uColR", layout::RIGHT_COLUMN[0] + 0.5f, layout::RIGHT_COLUMN[1] - 0.9f, layout::RIGHT_COLUMN[2] - 0.9f, layout::RIGHT_COLUMN[3] - 0.5f);
        ugl::drawFullscreen();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_BLEND);
    // ---- The well: its pale oak frame, the stacks, the cleared row, the ghost (none in a break)
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    if (!f.bare) {
        std::vector<Inst> frame;
        auto fb = [&](const vec3& c, const vec3& h, const vec3& col, int mat, float p0 = 0.f, float p1 = 0.f, float p2 = 0.f) {
            Inst in{};
            in.c[0] = c.x; in.c[1] = c.y; in.c[2] = c.z; in.c[3] = (float)mat;
            in.x[0] = h.x; in.x[3] = p0;
            in.y[1] = h.y; in.y[3] = p1;
            in.z[2] = h.z; in.z[3] = p2;
            in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = 0.4f;
            in.e[0] = -100.f; in.e[1] = -1.f; in.e[2] = 0.f;
            frame.push_back(in);
        };
        // a chunky painted frame
        for (int s = -1; s <= 1; s += 2) fb(vec3(s * 5.2f, -0.1f, -0.4f), vec3(0.17f, 10.68f, 0.45f), P.oak, M_WOOD);
        fb(vec3(0.f, -10.64f, -0.4f), vec3(5.37f, 0.14f, 0.45f), P.oak * 0.92f, M_WOOD);
        glUseProgram(progSolid_);
        common(progSolid_, true);
        lighting(progSolid_, true);
        set1f(progSolid_, "uBoard", 1.f);
        set1f(progSolid_, "uWorld", 0.f);
        set1f(progSolid_, "uStackA", 0.f);
        drawInst(dynVao_[MESH_BOX], dynVbo_[MESH_BOX], MESH_BOX, &frame, frame.size());
        set1f(progSolid_, "uStackA", 1.f);
        if (!cells_.empty()) drawInst(dynVao_[MESH_BOX], dynVbo_[MESH_BOX], MESH_BOX, &cells_, cells_.size());
        if (!fxInst_.empty()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            set1f(progSolid_, "uPremul", 1.f);
            drawInst(dynVao_[MESH_BOX], dynVbo_[MESH_BOX], MESH_BOX, &fxInst_, fxInst_.size());
            set1f(progSolid_, "uPremul", 0.f);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
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
    set1f(progDown_, "uThr", P.light ? 1.1f : 0.75f);
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
    set1f(progFinal_, "uBloomK", (0.45f + 0.1f * wp + 0.12f * dropAmt_) * (1.f + 0.4f * slow_));
    set1f(progFinal_, "uExposure", P.exposure * lerpf(0.35f, 1.f, std::min(smoothstepf(0.f, 0.8f, f.setup), 1.f - smoothstepf(0.f, 1.f, f.teardown))));
    set1f(progFinal_, "uSatF", 1.05f + 0.04f * wp + 0.05f * dropAmt_);
    set1f(progFinal_, "uSlow", slow_ * 0.5f);
    set3f(progFinal_, "uSlowTint", slowTint);
    set3f(progFinal_, "uLift", vec3(0.f));
    {
        // the HUD's cards: rounded, light, see-through (the room shows through them), an opaque edge in the accent
        auto rect = [&](float x0, float y0, float x1, float y1) {
            const vec2 a = project(vec3(x0, y1, 0.f)), b = project(vec3(x1, y0, 0.f)); // (pixels, top-left origin)
            return vec4(a.x, (float)height_ - b.y, b.x, (float)height_ - a.y);
        };
        const vec4 rl = rect(layout::LEFT_COLUMN[0] + 0.9f, layout::LEFT_COLUMN[1] + 0.3f, layout::LEFT_COLUMN[2] - 0.5f, layout::LEFT_COLUMN[3] - 0.5f);
        const vec4 rr = rect(layout::RIGHT_COLUMN[0] + 0.5f, layout::RIGHT_COLUMN[1] - 0.9f, layout::RIGHT_COLUMN[2] - 0.9f, layout::RIGHT_COLUMN[3] - 0.5f);
        set4f(progFinal_, "uCardL", rl.x, rl.y, rl.z, rl.w);
        set4f(progFinal_, "uCardR", rr.x, rr.y, rr.z, rr.w);
        set3f(progFinal_, "uCardCol", P.card);
        set3f(progFinal_, "uCardEdgeC", P.cardEdge);
        set1f(progFinal_, "uCardA", 0.f); // (the cards are laid in the scene, under the previews)
        set1f(progFinal_, "uCardRad", 26.f * height_ / 1080.f);
        set1f(progFinal_, "uCardEdgeW", std::max(1.5f, 3.f * height_ / 1080.f));
    }
    set1f(progFinal_, "uVig", P.light ? 0.06f : 0.2f);
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
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_library() {
    static const UniverseEntry e{
        "LIBRARY", "the blocks are books; each cleared line sends three big books to a cosy toon library room, a diorama that fills over one song, in 3D",
        [] { return std::unique_ptr<Universe>(new LibraryUniverse()); },
        // A calm, bright world: suits calm and mid-tempo songs best; fits most.
        [](const SongLook& l) { return 0.8f + 0.2f * (1.f - l.density) + 0.1f * (1.f - l.bassWeight); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
