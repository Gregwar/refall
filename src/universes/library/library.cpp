// LIBRARY: the blocks are books, and every cleared row is shelved in a contemporary library atrium that fills, bay
// after bay and floor after floor, with the game.
//
// The board stays a clean Tetris board: each cell is one neat block in its piece's color, a bound volume with its
// spine toward us (a soft bevel, two fine rules, a small label, its page edge on one side), over a calm ground. When
// a row clears, its blocks dissolve into light where they stand, and each cleared cell's books fly to the shelves in
// the background: from behind the cell's place on the board, at a depth behind the well, only ever going deeper
// (never in front of the board or the HUD). On the way they become a little stack of real books, which lands in the
// next free place on the shelves with a soft glow: shelved in the order they come, never moved again, so the
// collection is the history of the game. The shelves keep to quiet bindings, the pieces' colors here and there.
//
// The atrium is all there from the start: pale oak shelving bays on four floors, on the back wall either side of a
// tall glazed slot behind the well and down both side walls, the upper floors on galleries with glass balustrades
// round the void, a skylight over it all, reading tables on the ground floor. Each floor's light strips and
// downlights come on as it starts to fill; the camera frames the bays in use, easing back, then rising floor by
// floor toward the skylight.
//
// The song: dust in the daylight; the lights follow the music; a real drop runs a light along the collection, in
// the order it came, the strips breathing on the beat; a Tetris runs one light along it. Slow motion: blue hour.
// Game over: the blocks topple out of the bottom of the well.
//
// Layout contract: the board is drawn through the reference camera raised a little and shifted back down (the board
// plane keeps its pixels, the books' tops show); the library has its own camera behind it.
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

// A variant (sRGB): the seven bindings (I O T S Z J L); the oak, the walls, the slabs, the floor, the steel, the
// felt, the paper; the sky (high, low), the view outside; the ambient light (sky, ground), the sun, the strips; the
// HUD's veil, ink, accent and halo.
struct VariantDef {
    const char* name;
    unsigned piece[7];
    unsigned oak, wall, slab, floor, steel, felt, paper, skyA, skyB, outside, ambSky, ambGround, key, led;
    unsigned veil, ink, accent, halo;
    float night, keyK, ambK, ledK, exposure, floorKind;
    vec3 keyDir;
    bool light;
};
const VariantDef VARIANTS[] = {
    // pale oak, warm white and polished concrete in the afternoon sun; vermilion for the signage
    {"DAYLIGHT",
     {0x1f9fb5, 0xf0b31a, 0x7356cf, 0x2b9a66, 0xe2462f, 0x2754cf, 0xee7d2b},
     0xd8b88e, 0xf2efe9, 0xf5f3ee, 0xd9d3c8, 0x2a2c30, 0x3b4048, 0xf5f0e6, 0x7fb0e6, 0xdbe7f0, 0x7d9a70, 0xc8d4e2, 0xb4a690,
     0xfff0d8, 0xffe0b4,
     0xf7f4ef, 0x1b1c1f, 0xe2462f, 0xfbf9f6,
     0.f, 3.4f, 0.45f, 0.55f, 0.9f, 2.f, vec3(-0.3f, 0.9f, 0.32f), true},
    // everything white, cool bright light, the bindings in pure primaries; cobalt for the signage
    {"WHITE CUBE",
     {0x00a0c6, 0xffc20a, 0x8a3fd6, 0x15a957, 0xee2d2a, 0x1c4ce0, 0xff7417},
     0xeeede9, 0xf6f6f3, 0xfbfbf9, 0xe1dfda, 0x1b1c1e, 0x2c2f35, 0xf7f5f0, 0x8ab8ee, 0xe6eef6, 0x8aa596, 0xd6dde6, 0xc8c4bc,
     0xf6f9ff, 0xf2f5ff,
     0xfafafa, 0x111214, 0x1c4ce0, 0xffffff,
     0.f, 2.0f, 1.0f, 0.4f, 0.86f, 0.f, vec3(-0.45f, 0.72f, 0.52f), true},
    // birch and oak floors under a soft northern sky, the bindings in muted design colors; terracotta for the signage
    {"BIRCH",
     {0x4f97a3, 0xd9a93f, 0x86659f, 0x6f9864, 0xcc5b45, 0x3a5d8c, 0xdc8552},
     0xe6d3b2, 0xefece5, 0xf0ede6, 0xd6bd96, 0x34363a, 0x4a5058, 0xf3eee4, 0xb9c4cf, 0xe3e6e8, 0x6f7f72, 0xd9dde1, 0xc4b8a6,
     0xf4f3ef, 0xffe6c2,
     0xf4f1eb, 0x22252a, 0xcc5b45, 0xf8f6f2,
     0.f, 2.1f, 0.62f, 0.6f, 0.88f, 1.f, vec3(-0.25f, 0.88f, 0.4f), true},
    // evening: the city lit outside, the shelves glowing warm, oak and dark concrete; coral for the signage
    {"EVENING",
     {0x2cc0d4, 0xf5c242, 0x9767e6, 0x3abd78, 0xef4e48, 0x4a7bf0, 0xf78f3a},
     0xc89c6c, 0xe7e2da, 0xe3ded6, 0x56524e, 0x1c1d20, 0x2a2d33, 0xf2ece0, 0x101a33, 0x2c3b60, 0x141a26, 0x2a3346, 0x2a2219,
     0x9fb2dc, 0xffc684,
     0x15171b, 0xf3efe7, 0xff6b4f, 0x0d0e11,
     1.f, 0.3f, 0.8f, 1.7f, 1.08f, 0.f, vec3(-0.5f, 0.7f, 0.5f), false},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float BOARD_EYE_Y = 4.5f;  // the board camera's height (the books' tops show)
constexpr float FX_T = 0.27f;        // a cleared row dissolves in this long (the stack above waits 0.3 s)

// Test hooks: ZEN_LIBRARY_LINES=N adds N lines (their stacks already on the shelves: for screenshots),
// ZEN_GROW_RATE=R adds R lines a second (each one shelved like a real line: its ten stacks grow on the shelves).
// Each cleared cell's books go on the shelves as two stacks side by side (so some 100 lines fill the atrium).
constexpr int STACKS_CELL = 2;
uint32_t stackSeed(uint32_t cell, int k) { return k == 0 ? cell : hash32((uint64_t)cell * 2654435761ull + (uint64_t)k); }

int startLines() {
    static const int n = std::getenv("ZEN_LIBRARY_LINES") ? std::atoi(std::getenv("ZEN_LIBRARY_LINES")) : 0;
    return n;
}
int rateLines(double t) {
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return (int)(rate * t);
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
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "library text");

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
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
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

    buildFont(reg_, FONT_REG);
    buildFont(semi_, FONT_SEMI);
    buildFont(ital_, FONT_ITAL);
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
    for (GLuint v : {spriteVao_, lineVao_, shapeVao_, textVao_, glassVao_, shelfVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, lineVbo_, shapeVbo_, textVbo_, glassVbo_, shelfVbo_}) glDeleteBuffers(1, &b);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    if (shadowTex_) glDeleteTextures(1, &shadowTex_);
    for (Font* f : {&reg_, &semi_, &ital_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
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
    for (int i = 0; i < 7; i++) P.piece[i] = lin(V.piece[i]);
    P.oak = lin(V.oak); P.wall = lin(V.wall); P.slab = lin(V.slab); P.floor = lin(V.floor); P.steel = lin(V.steel); P.felt = lin(V.felt);
    P.paper = lin(V.paper); P.skyA = lin(V.skyA); P.skyB = lin(V.skyB); P.outside = lin(V.outside);
    P.ambSky = lin(V.ambSky) * V.ambK; P.ambGround = lin(V.ambGround) * V.ambK; P.key = lin(V.key) * V.keyK; P.led = lin(V.led);
    P.veil = lin(V.veil); P.ink = hex(V.ink); P.accent = hex(V.accent); P.halo = hex(V.halo);
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
// The stacks
// =====================================================================================================

// A cell's stack, from its seed: a pile lying flat (the longest at the bottom) or a row standing, neat.
LibraryUniverse::Stack LibraryUniverse::makeStack(uint32_t seed) {
    Stack s;
    Rng r(0x51AC0000ull ^ ((uint64_t)seed * 2654435761ull));
    const float B = -0.47f;
    if (r.chance(0.52f)) {
        const float w[5] = {0.f, 0.f, 0.3f, 0.45f, 0.25f};
        s.n = r.weighted(w);
        const float total = r.range(0.8f, 0.94f);
        float t[5], L[5], sum = 0.f;
        for (int i = 0; i < s.n; i++) { t[i] = r.range(0.65f, 1.35f); sum += t[i]; L[i] = r.range(0.78f, 0.94f); }
        std::sort(L, L + s.n, [](float a, float b) { return a > b; });
        float y = B;
        for (int i = 0; i < s.n; i++) {
            const float th = t[i] / sum * total;
            BookDef& b = s.b[i];
            b.lying = true;
            b.hx = L[i] * 0.5f;
            b.hy = th * 0.5f - 0.006f;
            b.cy = y + th * 0.5f;
            b.cx = r.range(-1.f, 1.f) * (0.47f - b.hx) * 0.7f;
            b.hd = r.range(0.3f, 0.37f);
            b.z = r.range(-0.035f, 0.f);
            y += th;
        }
        s.top = y;
    } else {
        const float w[6] = {0.f, 0.f, 0.f, 0.3f, 0.45f, 0.25f};
        s.n = r.weighted(w);
        const float total = r.range(0.86f, 0.94f);
        float t[5], sum = 0.f;
        for (int i = 0; i < s.n; i++) { t[i] = r.range(0.6f, 1.4f); sum += t[i]; }
        float x = -total * 0.5f;
        s.top = B;
        for (int i = 0; i < s.n; i++) {
            const float wd = t[i] / sum * total;
            BookDef& b = s.b[i];
            b.lying = false;
            b.hx = wd * 0.5f - 0.006f;
            b.cx = x + wd * 0.5f;
            const float H = r.range(0.72f, 0.94f);
            b.hy = H * 0.5f;
            b.cy = B + H * 0.5f;
            b.hd = r.range(0.3f, 0.37f);
            b.z = r.range(-0.035f, 0.f);
            x += wd;
            s.top = std::max(s.top, B + H);
        }
    }
    for (int i = 0; i < s.n; i++) {
        BookDef& b = s.b[i];
        b.tone = r.range(0.88f, 1.07f);
        b.style = r.irange(0, 3);
        b.seed = r.range(0.f, 10.f);
    }
    return s;
}

// The stack's books as instances: in the cell's frame (origin its center, along / up / out its axes, k its size),
// grown from its foot by `grow`, swollen by a clear's fade.
void LibraryUniverse::pushStack(std::vector<Inst>& out, const Stack& s, int type, const vec3& origin, const vec3& along, const vec3& up,
                                const vec3& outD, float k, float grow, float glow, float fade, bool calm) {
    if (grow <= 0.001f) return;
    const vec3 base = pieceColor(type);
    // on the shelves: mostly quiet bindings (paper, stone, taupe, charcoal, kraft), the piece's color here and there
    static const vec3 NEUTRAL[5] = {lin(0xe9e4d8), lin(0xc4bcae), lin(0x8d857a), lin(0x57544f), lin(0xb39a76)};
    const vec3 foot = origin - up * (0.47f * k);
    const float swell = 1.f + 0.14f * (1.f - (1.f - fade) * (1.f - fade));
    for (int i = 0; i < s.n; i++) {
        const BookDef& b = s.b[i];
        vec3 c = origin + along * (b.cx * k) + up * (b.cy * k) + outD * ((b.z - b.hd) * k);
        float g = grow;
        if (fade > 0.f) { c = origin + (c - origin) * swell; g = swell; }
        else c = foot + (c - foot) * grow;
        const vec3 X = b.lying ? up * (b.hy * k * g) : along * (b.hx * k * g);
        const vec3 Y = b.lying ? along * (b.hx * k * g) : up * (b.hy * k * g);
        const vec3 Z = outD * (b.hd * k * g);
        vec3 col = base * b.tone;
        if (calm) {
            const uint32_t h = hash32((uint64_t)(b.seed * 100003.f) + (uint64_t)i * 7919ull);
            col = (h & 255) < 72 ? base * b.tone * 0.92f : NEUTRAL[(h >> 8) % 5] * b.tone;
        }
        Inst in;
        in.c[0] = c.x; in.c[1] = c.y; in.c[2] = c.z; in.c[3] = (float)M_BOOK;
        in.x[0] = X.x; in.x[1] = X.y; in.x[2] = X.z; in.x[3] = (float)b.style;
        in.y[0] = Y.x; in.y[1] = Y.y; in.y[2] = Y.z; in.y[3] = glow;
        in.z[0] = Z.x; in.z[1] = Z.y; in.z[2] = Z.z; in.z[3] = fade;
        in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = b.seed;
        in.e[0] = -100.f; in.e[1] = -1.f; in.e[2] = stLevel_; in.e[3] = -10.f;
        out.push_back(in);
    }
}

void LibraryUniverse::pushCell(std::vector<Inst>& out, int type, const vec3& c, const vec3& along, const vec3& up, float k, float fade,
                               float seed) {
    const float sw = 1.f + 0.05f * fade;
    const vec3 X = along * (0.455f * k * sw), Y = up * (0.455f * k * sw), Z = cross(along, up) * (0.34f * k * sw);
    const vec3 cc = c - Z;
    const vec3 col = pieceColor(type);
    Inst in;
    in.c[0] = cc.x; in.c[1] = cc.y; in.c[2] = cc.z; in.c[3] = (float)M_CELL;
    in.x[0] = X.x; in.x[1] = X.y; in.x[2] = X.z; in.x[3] = 0.f;
    in.y[0] = Y.x; in.y[1] = Y.y; in.y[2] = Y.z; in.y[3] = 0.f;
    in.z[0] = Z.x; in.z[1] = Z.y; in.z[2] = Z.z; in.z[3] = fade;
    in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = seed;
    in.e[0] = -100.f; in.e[1] = -1.f; in.e[2] = -1.f; in.e[3] = -10.f;
    out.push_back(in);
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

// A slot on the shelves: floor by floor, bay by bay (bayFrame's order), shelf by shelf from the bottom, four stacks
// a shelf. Its frame: the stack's center over the shelf, at the shelf's front.
void LibraryUniverse::slotFrame(int slot, vec3& origin, vec3& along, vec3& out) const {
    const int level = slot / SLOTS_LEVEL;
    const int r = slot % SLOTS_LEVEL;
    const BayFrame b = bayFrame(r / SLOTS_BAY);
    const int r2 = r % SLOTS_BAY;
    const int shelf = r2 / SLOTS, pos = r2 % SLOTS;
    const float x = -(BAY_W * 0.5f - 2.f * UPRIGHT) + (pos + 0.5f) * SLOT_W;
    const float jit = ((hash32((uint64_t)slot * 7919ull) & 1023) / 1023.f - 0.5f) * 0.03f;
    origin = b.foot + b.along * (x + jit) + UP * (level * LEVEL_H + SHELF_Y0 + shelf * SHELF_PITCH + 0.47f * BOOK_K) - b.out * 0.035f;
    along = b.along;
    out = b.out;
}

void LibraryUniverse::fly(int type, uint32_t seed, const vec3& board, float delay) {
    if (nextSlot_ >= SLOTS_ALL) return;
    shelve(type, seed, 0.f);
    groups_.back().t0 = 1e9f; // (not on its shelf until it lands)
    Flight fl;
    fl.group = (int)groups_.size() - 1;
    fl.board = board;
    fl.t = -delay;
    flights_.push_back(fl);
}

void LibraryUniverse::shelve(int type, uint32_t seed, float delay) {
    if (nextSlot_ >= SLOTS_ALL) return;
    Group g;
    g.type = type;
    g.seed = seed;
    g.slot = nextSlot_++;
    g.t0 = worldTime_ + delay;
    groups_.push_back(g);
}

void LibraryUniverse::resetGrowth() {
    groups_.clear();
    flights_.clear();
    nextSlot_ = 0;
    shelfInst_.clear();
    settled_ = 0;
    shelfDirty_ = true;
    camDs_ = -1.f;
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
        if (ev.count >= 4) tetrisT_ = 0.f;
        std::vector<GameEvent::CellInfo> cs;
        for (auto& c : ev.cells)
            if (c.x >= 0 && c.x < Game::W && c.y >= 0 && c.y < Game::H) cs.push_back(c);
        // rows bottom first, each left to right: the order they go on the shelves
        std::sort(cs.begin(), cs.end(), [](const GameEvent::CellInfo& a, const GameEvent::CellInfo& b) { return a.y != b.y ? a.y > b.y : a.x < b.x; });
        bool full[Game::H] = {};
        int i = 0;
        for (auto& c : cs) {
            const int type = std::clamp(c.type, 0, 6);
            const uint32_t sd = cellSeed(c.x, c.y, type);
            full[c.y] = true;
            const vec3 bp = layout::cellPos((float)c.x, (float)c.y);
            if (c.y >= Game::HIDDEN) fx_.push_back({type, sd, bp, 0.f});
            for (int k = 0; k < STACKS_CELL; k++) fly(type, stackSeed(sd, k), bp, 0.05f + 0.06f * i + 0.03f * k);
            i++;
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
            pushCell(cells_, c.type, p, X, UP, 1.f, 0.f, (float)(cellSeed(x, y, c.type) & 1023) / 97.f);
        }
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++)
            pushCell(cells_, p.type, layout::cellPos(cells[i][0] + dx, cells[i][1] + dy), X, UP, 1.f, 0.f,
                     (float)(hash32(((uint64_t)spawnN_ << 3) + (uint64_t)i) & 1023) / 97.f);
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
            const vec3 col = lerp(ugl::toLinear(pal_.ink), pieceColor(p.type), 0.6f);
            const float wpx = std::max(1.f, 1.6f * height_ / 1080.f);
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
            pushCell(cells_, type, vec3(center.x + ox * sc, center.y - oy * sc, 0.f), X, UP, sc, 0.f, 1.3f * i + type);
        }
    };
    if (!g.over()) {
        preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0));
        for (int i = 0; i < 3; i++) {
            const vec3 c = layout::nextCenter(i);
            if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i));
        }
    }
    stLevel_ = 0.f;
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
    // a clean grotesque: charcoal (or warm white at night) on a soft halo, the variant's one accent for the labels
    const vec3 ink = pal_.ink, halo = pal_.halo;
    const vec3 accent = pal_.accent;
    const float haloK = pal_.light ? 0.75f : 0.65f;
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
    // the bonus gauge: a hairline ring, its charge a solid arc in the accent (none in a break)
    if (vis > 0.01f && !bare_) {
        const float s = height_ / 1080.f;
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 120;
        const float th = std::max(1.f, 1.3f * s), th2 = std::max(3.f, 5.f * s);
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
        for (int i = 0; i < N; i++) seg(TAU * i / N, TAU * (i + 1) / N, r, th, ink, (g.bonusReady() ? 0.6f : 0.3f) * dimk);
        flushShapes();
        for (int i = 0; i < N; i++) {
            if ((i + 0.5f) / N > fill) break;
            seg(TAU * i / N, TAU * (i + 1) / N, r, th2, accent, 0.95f * dimk);
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
            place(semi_, upper(h.text), base * 0.66f, base * 0.16f, base * 0.06f);
            drawText(semi_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f) * 0.85f * vis, haloK);
            continue;
        case HUD_SECTION:
            place(semi_, upper(h.text), base * 0.72f, base * 0.22f);
            drawText(semi_, q, accent, halo, saturate(0.3f + h.alpha * 1.7f) * vis, haloK);
            continue;
        case HUD_TITLE:
            place(ital_, h.text, base * 1.06f, 0.f, -base * 0.08f);
            drawText(ital_, q, ink, halo, saturate(h.alpha * 1.5f) * vis, haloK);
            continue;
        case HUD_INFO:
            place(reg_, upper(h.text), base * 0.62f, base * 0.14f);
            drawText(reg_, q, ink, halo, saturate(alpha * 1.5f) * 0.85f, haloK);
            continue;
        case HUD_LABEL:
            place(semi_, upper(h.text), base * 0.68f, base * 0.24f);
            drawText(semi_, q, accent, halo, saturate(alpha * 1.3f) * 0.95f, haloK);
            continue;
        case HUD_VALUE:
            place(reg_, h.text, base * 1.02f, base * 0.01f, -base * 0.08f);
            drawText(reg_, q, ink, halo, alpha * 0.96f, haloK);
            continue;
        case HUD_NOTICE:
            place(semi_, h.text, base * 0.9f, base * 0.02f);
            drawText(semi_, q, accent, halo, alpha, haloK);
            continue;
        case HUD_KEY:
            place(semi_, h.text, base * 0.7f, base * 0.08f);
            drawText(semi_, q, ink, halo, alpha, haloK);
            continue;
        case HUD_TOAST:
            place(semi_, upper(h.text), base * 0.86f, base * 0.24f, -base * 0.1f);
            drawText(semi_, q, ink, halo, alpha, 0.65f);
            continue;
        case HUD_TOAST_SUB:
            place(reg_, h.text, base * 0.95f, base * 0.02f);
            drawText(reg_, q, accent, halo, alpha * 0.95f, 0.65f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * height_ / 1080.f) {
                place(semi_, upper(h.text), base * 0.8f, base * 0.22f);
                drawText(semi_, q, ink, halo, alpha, 0.75f);
            } else {
                place(reg_, h.text, base * 0.92f, base * 0.01f);
                drawText(reg_, q, h.accent ? accent : ink, halo, alpha, 0.75f);
            }
            continue;
        default:
            place(reg_, h.text, base * 0.7f, base * 0.04f);
            drawText(reg_, q, h.accent ? accent : ink, halo, alpha, haloK);
            continue;
        }
    }
}

// =====================================================================================================
// The camera
// =====================================================================================================

// Framed on the bays in use: the first two at eye level at the start, easing back as the bays fill outward, then
// back and up as the collection climbs the floors.
void LibraryUniverse::updateCamera(float dt) {
    const float tw = std::tan(25.f * PI / 180.f);
    tanW_ = tw;
    const int used = std::max(1, nextSlot_);
    const int level = std::min(LEVELS - 1, (used - 1) / SLOTS_LEVEL);
    const float bays = (float)(used - level * SLOTS_LEVEL) / SLOTS_BAY; // bays in use on this floor (fractional)
    const float zb = ZBACK + CASE_D;
    // the back wall: as wide as its bays in use; then down the side walls: back far enough to see the newest bays
    float D;
    if (level == 0 && bays <= 2 * BACK_BAYS) {
        const float perSide = std::min((float)BACK_BAYS, std::max(1.f, std::ceil(bays * 0.5f + 0.2f)));
        D = (SLOT_HALF + perSide * BAY_W + 0.35f) / (tw * aspect_);
    } else if (level == 0) {
        const float sideUsed = std::max(0.f, bays - 2 * BACK_BAYS) * 0.5f;
        const float zf = SIDE_Z0 + std::min((float)SIDE_BAYS, std::ceil(sideUsed + 0.3f)) * BAY_W;
        D = zf - zb + HALL_HALF / (tw * aspect_) * 0.8f;
    } else D = (HALL_HALF + 0.3f) / (tw * aspect_);
    // up the floors: the one being filled and the one under it (the skylight over the top one)
    const float yTop = level == 0 ? LEVEL_H - 0.25f : (level + 1) * LEVEL_H + (level == LEVELS - 1 ? 0.5f : -0.2f);
    const float yBot = level <= 1 ? -0.15f : (level - 1) * LEVEL_H + 0.3f;
    D = std::max({4.2f, D, ((yTop - yBot) * 0.5f + 0.2f) / tw});
    // the eye stays low on the ground floor, rising with the floors in use
    const float ty = level == 0 ? std::min(1.6f + 0.06f * (D - 4.2f), 2.4f) : (yTop + yBot) * 0.5f;
    const vec3 T(0.f, ty, zb);
    if (camDs_ < 0.f) { camDs_ = D; camT_ = T; camDv_ = 0.f; camTv_ = vec3(0.f); }
    const float w = 0.9f;
    camDv_ += (w * w * (D - camDs_) - 2.f * w * camDv_) * dt;
    camDs_ += camDv_ * dt;
    camTv_ = camTv_ + ((T - camT_) * (w * w) - camTv_ * (2.f * w)) * dt;
    camT_ = camT_ + camTv_ * dt;
    float yaw = 0.008f * std::sin(worldTime_ * 0.029f), pitch = 0.006f * std::sin(worldTime_ * 0.041f);
    fwdW_ = vec3(-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch));
    rightW_ = normalize(cross(fwdW_, UP));
    upW_ = cross(rightW_, fwdW_);
    targetW_ = camT_;
    camD_ = camDs_;
    eyeW_ = camT_ - fwdW_ * camDs_;
    const float zn = std::max(0.05f, camD_ * 0.01f), zf = std::max(80.f, camD_ * 8.f);
    const mat4 view = mat4::lookAt(vec3(0.f), fwdW_, UP);
    vpW_ = mat4::perspective(2.f * std::atan(tanW_), aspect_, zn, zf) * view;
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
    // ZEN_LIBRARY_TIMING=1: the frame's cost (GPU included: glFinish around it), every 200 frames
    static const bool timing = std::getenv("ZEN_LIBRARY_TIMING") != nullptr;
    struct Timer {
        bool on;
        double t0 = 0;
        const LibraryUniverse* u;
        Timer(bool o, const LibraryUniverse* uu) : on(o), u(uu) {
            if (on) { glFinish(); t0 = now(); }
        }
        static double now() {
            timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
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
    tetrisT_ += dt;
    kickK_ = std::max(kickK_ * std::exp(-dt * 5.f), kickIn_);
    kickIn_ = 0.f;
    refreshK_ = approach(refreshK_, 0.f, 0.5f, dt);
    beatPulse_ = music.hasSong ? std::pow(1.f - (float)(music.beat - std::floor(music.beat)), 3.f) : 0.f;
    if (!f.bare) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- The game's lines -> the collection (a new game empties the shelves).
    {
        const int hookStart = startLines(), hookRate = rateLines(time_);
        const int lines = f.bare ? lines_ : linesHere(game) + hookStart + hookRate; // (a break: the shelves as they were)
        if (linesSeen_ >= 0 && lines < lines_ && !game.over()) resetGrowth();
        lines_ = lines;
        linesSeen_ = lines;
        // the test hooks' lines: the static offset's stacks put straight on the shelves (screenshots), the grow
        // rate's lines shelved like real ones (ten stacks growing on the shelves, one after the other)
        if (!hookPlaced_) {
            hookPlaced_ = true;
            Rng hr(0xB00C5ull);
            for (int l = 0; l < hookStart; l++)
                for (int k = 0; k < Game::W * STACKS_CELL; k++) {
                    shelve(hr.irange(0, 6), (uint32_t)hr.next(), 0.f);
                    groups_.back().t0 = -100.f;
                }
        }
        while (hookLines_ < hookRate) {
            hookLines_++;
            for (int x = 0; x < Game::W; x++) {
                const int type = rng_.irange(0, 6);
                const uint32_t sd = (uint32_t)rng_.next();
                for (int k = 0; k < STACKS_CELL; k++)
                    fly(type, stackSeed(sd, k), layout::cellPos((float)x, (float)(Game::H - 1)), 0.05f + 0.06f * x + 0.03f * k);
            }
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
    // each floor's lights: on once it starts to fill
    {
        const int cur = nextSlot_ > 0 ? std::min(LEVELS - 1, (nextSlot_ - 1) / SLOTS_LEVEL) : 0;
        for (int k = 0; k < 8; k++) levelOn_[k] = approach(levelOn_[k], k <= cur ? 1.f : 0.f, 0.6f, dt);
    }

    // ---- Dynamic instances: the collection's stacks.
    for (auto& v : dyn_) v.clear();
    cells_.clear();
    fxInst_.clear();
    cellLines_.clear();
    sprites_.clear();
    const int nG = (int)groups_.size();
    auto pushGroup = [&](std::vector<Inst>& out, int i, float grow, float glow) {
        const Group& g = groups_[i];
        vec3 o, al, ou;
        slotFrame(g.slot, o, al, ou);
        stLevel_ = (float)(g.slot / SLOTS_LEVEL);
        const size_t first = out.size();
        pushStack(out, makeStack(g.seed), g.type, o, al, UP, ou, BOOK_K, grow, glow, 0.f, true);
        // its place in the collection (the drop's and the Tetris' light run along it)
        for (size_t j = first; j < out.size(); j++) out[j].e[3] = (float)i;
    };
    // settled stacks (grown, their light faded) go to the kept buffer, the others are drawn as they grow
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
        // growing onto its shelf from its foot, a soft light fading as it settles
        pushGroup(dyn_[MESH_BOX], i, a > 0.6f ? 1.f : easeOutBack(a / 0.5f), 0.6f * std::exp(-std::max(0.f, a - 0.6f) * 1.4f));
    }
    stLevel_ = 0.f;

    // ---- Flights: a cleared cell's books, from behind its place in the well to their shelf, only ever going deeper
    for (auto it = flights_.begin(); it != flights_.end();) {
        Flight& fl = *it;
        fl.t += wdt;
        if (fl.group < 0 || fl.group >= (int)groups_.size()) { it = flights_.erase(it); continue; }
        Group& g = groups_[fl.group];
        if (fl.t < 0.f) { ++it; continue; }
        vec3 oT, alT, ouT;
        slotFrame(g.slot, oT, alT, ouT);
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
            fl.k0 = 2.f * halfNdc * d0 * tanW_;
            fl.out0 = normalize(vec3(-fwdW_.x, 0.f, -fwdW_.z));
            fl.dur = 0.95f + 0.22f * std::log2(1.f + length(oT - fl.p0) / 2.f);
        }
        const float s = saturate(fl.t / fl.dur);
        if (s >= 1.f) {
            g.t0 = worldTime_ - 0.6f; // on its shelf, full size, its light fading from now
            it = flights_.erase(it);
            continue;
        }
        if (!f.bare) {
            const float e = s * s * (3.f - 2.f * s);
            const float L = length(oT - fl.p0);
            const vec3 p = lerp(fl.p0, oT, e) + UP * (0.12f * L * std::sin(PI * e));
            const vec3 ou = normalize(lerp(fl.out0, ouT, smoothstepf(0.f, 0.8f, s)));
            const vec3 al = normalize(cross(UP, ou));
            stLevel_ = (float)(g.slot / SLOTS_LEVEL);
            pushStack(dyn_[MESH_BOX], makeStack(g.seed), g.type, p, al, UP, ou, lerpf(fl.k0, BOOK_K, e), 1.f, 0.25f, 0.f, true);
            stLevel_ = 0.f;
        }
        ++it;
    }

    // ---- A cleared row: its stacks swell and dissolve into light where they stand.
    for (auto it = fx_.begin(); it != fx_.end();) {
        it->t += dt;
        if (it->t > FX_T) { it = fx_.erase(it); continue; }
        if (!f.bare && it->t > 0.f) {
            const float s = saturate(it->t / FX_T);
            pushCell(fxInst_, it->type, it->p, vec3(1, 0, 0), UP, 1.f, std::max(0.02f, s), 0.f);
        } else if (!f.bare) {
            pushCell(cells_, it->type, it->p, vec3(1, 0, 0), UP, 1.f, 0.f, 0.f);
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
            pushCell(cells_, t.type, p, al, up, 1.f, 0.f, 0.f);
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
            const float on = levelOn_[std::clamp(l.level, 0, 7)];
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
            LC.push_back(vec4(l.col, l.k * lampLevel * levelOn_[std::clamp(l.level, 0, 7)] * 0.9f));
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
    const vec3 fogCol = lerp(P.wall, P.skyB, 0.4f) * (P.light ? 0.9f : 0.35f);
    // the collection's light: the drop runs it slowly and on, a Tetris once along it
    float dropT = -10.f, dropK = 0.f;
    if (tetrisT_ < 3.f) { dropT = tetrisT_ / 2.4f * 1.3f - 0.15f; dropK = 0.55f; }
    else if (dropAmt_ > 0.01f) { dropT = std::fmod(worldTime_ * 0.07f, 1.4f) - 0.2f; dropK = 0.45f * dropAmt_ * (0.75f + 0.25f * beatPulse_); }
    auto lighting = [&](GLuint p, bool board) {
        if (board) {
            set3f(p, "uAmbSky", lerp(P.ambSky, vec3(0.6f), 0.4f) * (P.light ? 0.85f : 0.75f));
            set3f(p, "uAmbGround", lerp(P.ambGround, vec3(0.35f), 0.4f) * 0.8f);
            set3f(p, "uKeyDir", normalize(vec3(-0.45f, 0.72f, 0.6f)));
            set3f(p, "uKeyCol", vec3(1.f, 0.97f, 0.92f) * (1.35f + 0.12f * wp + 0.2f * dropAmt_) * lerp(vec3(1.f), slowTint, slow_));
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
            set1f(p, "uFogD", 0.35f / std::max(6.f, camD_ * 3.f));
            set1f(p, "uFogStart", camD_ * 0.8f);
            set3f(p, "uFogCol", fogCol);
        }
    };
    auto common = [&](GLuint p, bool board) {
        set3f(p, "uEye", board ? eyeB_ : eyeW_);
        set1f(p, "uTime", worldTime_);
        setMat(p, "uVP", board ? vpB_ : vpW_);
        set3f(p, "uOrigin", board ? vec3(0.f) : eyeW_);
    };

    // ---- The sun's shadow map, round what the camera sees.
    const vec3 shC = targetW_;
    const float shR = std::max(5.f, camD_ * 1.1f);
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
    const float skyK = P.light ? 1.6f : 1.f;
    set3f(progSolid_, "uSkyA", lerp(P.skyA, P.skyA * slowTint * 0.45f, slow_) * skyK);
    set3f(progSolid_, "uSkyB", lerp(P.skyB, P.skyB * slowTint * 0.5f, slow_) * skyK);
    set3f(progSolid_, "uOutside", P.outside * (P.light ? 1.3f : 1.f) * (1.f - 0.4f * slow_));
    set3f(progSolid_, "uLed", P.led);
    set3f(progSolid_, "uPaper", P.paper);
    set1f(progSolid_, "uLampK", lampLevel);
    set1f(progSolid_, "uNight", P.night);
    set1f(progSolid_, "uWorld", 1.f);
    set1f(progSolid_, "uShelfLight", lampLevel);
    set1f(progSolid_, "uLevelH", LEVEL_H);
    set1f(progSolid_, "uBeat", beatPulse_);
    glUniform1fv(ugl::U(progSolid_, "uLevelOn"), 8, levelOn_);
    set1f(progSolid_, "uDropT", dropT);
    set1f(progSolid_, "uDropK", dropK);
    set1f(progSolid_, "uDropSpan", 0.06f);
    set1f(progSolid_, "uGroupN", (float)std::max<size_t>(1, groups_.size()));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    set1i(progSolid_, "uShadow", 1);
    setMat(progSolid_, "uShadowVP", shVP);
    set3f(progSolid_, "uShadowO", shC);
    set1f(progSolid_, "uShadowTexel", 1.f / 2048.f);
    set1f(progSolid_, "uShadowBias", 2.f * shR / 2048.f * 1.5f);
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
        // the well's ground: clean and calm (the light variants' a near white after the tone curve)
        set3f(progVeil_, "uVeil", P.veil * (P.light ? 1.45f : 1.f));
        set1f(progVeil_, "uWellA", P.light ? 0.94f : 0.96f);
        set1f(progVeil_, "uColA", P.light ? 0.5f : 0.62f);
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
        set4f(progVeil_, "uColR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
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
        for (int s = -1; s <= 1; s += 2) fb(vec3(s * 5.14f, -0.05f, -0.4f), vec3(0.12f, 10.6f, 0.42f), P.oak, M_WOOD);
        fb(vec3(0.f, -10.6f, -0.4f), vec3(5.26f, 0.1f, 0.42f), P.oak * 0.92f, M_WOOD);
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
    set1f(progFinal_, "uExposure", P.exposure);
    set1f(progFinal_, "uSatF", 1.02f + 0.05f * wp + 0.06f * dropAmt_);
    set1f(progFinal_, "uSlow", slow_ * 0.5f);
    set3f(progFinal_, "uSlowTint", slowTint);
    set3f(progFinal_, "uLift", vec3(0.f));
    set1f(progFinal_, "uVig", P.light ? 0.16f : 0.3f);
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
        "LIBRARY", "the blocks are little stacks of books, shelved full size in a light, modern library atrium that fills floor by floor, in 3D",
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
