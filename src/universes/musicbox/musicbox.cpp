// MUSIC BOX: a wooden cylinder music box under a warm lamp, and every cleared row pinned on its cylinder.
//
// The well stands in front of a Swiss cylinder music box on a table: a lacquered box with its lid open (the
// inside inlaid with stringing, a crossbanded border and a rosette), velvet lining, a black iron bedplate, a brass
// cylinder on its arbor between two cocks, the steel comb with its ten teeth (one per column of the
// well), the spring barrel with its ratchet and the great wheel on the left, the governor on the right (a
// gear train, the endless screw and its spinning fly). The pieces are plates of metal or enamel (brass, copper,
// nickel, blued steel, rose gold, enamels: a set per variant) each with a domed brass boss, the pins' heads.
//
// A cleared row: its ten cells lift out of the well and fly calmly to the cylinder, shrinking into ten pins along
// its length, one under each tooth, at the next free angle. Every pin on the cylinder is a cleared cell, never
// invented: N rows, 10 N pins. The cylinder turns with the song, one slot of pins past the comb per beat (half
// that in calm parts), so each pinned row plucks the ten teeth on a beat of the song: they lift, release and ring
// down. The cylinder holds SLOTS rows round its circumference and TUNES tunes side by side (offset a little along
// its axis, as on a real multi-tune cylinder): when a tune's slots are all pinned the next tune starts, and the
// cylinder shifts sideways between turns to play each pinned tune in turn. A full cylinder is lifted out and laid
// on the table beside the box, and a fresh one is set in.
//
// The song: calm is the still, warm lamp and a slow turn; mid and peak a livelier lamp and glints on the brass, the
// teeth sparkling as they ring at a peak. A real drop: the dancer's trap opens behind the cylinder and a small
// porcelain dancer rises and turns in a ring of light on the beat, then settles back over the following bars. A
// quad: a gleam runs round the pins. Slow motion: the governor slows and the light falls to evening. Game over:
// the box winds down. A level: the ratchet winds the spring.
//
// Layout contract: the board is drawn through the reference camera (raised a little and shifted back, so the
// plates' edges show); the box has its own camera behind it.
#include "universes/musicbox/musicbox.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/musicbox/shaders.hpp"
#include "universes/musicbox/world.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

using namespace mb_world;

namespace {

// Playfair Display (Roman, Italic), SIL Open Font License, see OFL-PlayfairDisplay.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/musicbox/PlayfairDisplay.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/musicbox/PlayfairDisplay-Italic.ttf.inc"
};

namespace shaders = musicbox_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

const vec3 UP(0.f, 1.f, 0.f), AX(1.f, 0.f, 0.f), AZ(0.f, 0.f, 1.f);
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }

// A light (sRGB): the seven pieces (I O T S Z J L) and which are enamel; the box's veneer, its lid's, the two
// inlay woods, the velvet, the brass (the mechanism and the bosses), the steel, the bedplate, the table, the wall,
// the lamp, the ambient (above, below), the fill (the window), the HUD's veil, ink and accent, porcelain.
struct VariantDef {
    const char* name;
    unsigned piece[7];
    int enamel[7];
    unsigned wood, woodLid, inlayA, inlayB, velvet, brass, steel, bed, table, wall, lamp, amb, ambLow, fill, veil, ink, accent, porcelain;
    float lampK, exposure, figure, night;
};
const VariantDef VARIANTS[] = {
    // a walnut box, burgundy velvet, brass, the lamp warm
    {"WALNUT & BRASS",
     {0x3e72b8, 0xe2b65a, 0xe8a894, 0x2f8a5c, 0xd2683a, 0xd2d6da, 0xa8282e},
     {0, 0, 0, 1, 0, 0, 1},
     0x6a4428, 0x5a3820, 0xead6a8, 0x24170e, 0x5a1020, 0xe0b45c, 0xc8ccd2, 0x23201d, 0x2c1b12, 0x2a1e17, 0xffc98a, 0x3a2e26,
     0x1c1410, 0x4a5670, 0x0a0806, 0xf2e6cc, 0xe8c070, 0xf4efe6, 1.f, 1.f, 0.f, 0.f},
    // black ebony, midnight velvet, nickel silver, a cooler lamp; enamels
    {"EBONY & NICKEL",
     {0x2a5ad0, 0xd8dde2, 0x8a52b8, 0x34a07a, 0xc8303a, 0xe0b048, 0xd47a48},
     {1, 0, 1, 1, 1, 0, 0},
     0x1a1410, 0x161210, 0xdfe2e6, 0x8a8f96, 0x121c40, 0xcfd2d8, 0xd8dce2, 0x18181c, 0x1c1714, 0x17171b, 0xffd8b0, 0x2c2c34,
     0x101014, 0x56688c, 0x050608, 0xeef0f4, 0xd4dae4, 0xf6f4f0, 1.05f, 1.05f, 0.f, 0.f},
    // rosewood with its dark figure, green velvet, gilt, the lamp low and golden
    {"ROSEWOOD & GILT",
     {0x1f9aa8, 0xf2c454, 0xe8a08c, 0x6a9a2e, 0xd8483a, 0x2c4aa8, 0xdadcde},
     {1, 0, 0, 1, 1, 1, 0},
     0x4e1d13, 0x481a11, 0xf0dcb0, 0x1a0a06, 0x173a26, 0xf0c45a, 0xd0d2d6, 0x221e1a, 0x221510, 0x241812, 0xffc584, 0x3a2a20,
     0x1a120e, 0x50506a, 0x080504, 0xf6e6c8, 0xf0c860, 0xf6efe2, 0.95f, 1.f, 1.f, 0.f},
    // pale birch at night: the moon at the window, the lamp turned low, midnight velvet
    {"BIRCH NIGHT",
     {0x4a9ad8, 0xdcb050, 0xb084d8, 0x58b088, 0xe0503a, 0x3054a0, 0xd27840},
     {1, 0, 1, 1, 1, 0, 0},
     0xb89a6c, 0xae9064, 0x5a3c26, 0x2a1a10, 0x1a2452, 0xdcb45a, 0xc8ccd4, 0x202228, 0x1c1a1a, 0x0c111c, 0xffc890, 0x1c2234,
     0x0c1018, 0x8aa6e0, 0x05070c, 0xe8eef8, 0xe2c474, 0xf4f2ee, 0.6f, 1.f, 0.f, 1.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float SLIDE_T = 0.3f;      // a cleared cell lifts out of the well in this long, then flies
constexpr float CELL_H = 0.47f;      // a plate in the well: its half width, half depth
constexpr float CELL_D = 0.22f;
constexpr float BOARD_EYE_Y = 3.f;   // the board camera's height (the plates' top edges show)

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
// x wrapped into [-n/2, n/2)
double wrapd(double x, double n) {
    x = std::fmod(x + n * 0.5, n);
    if (x < 0) x += n;
    return x - n * 0.5;
}
// A rotation of v about a unit axis.
vec3 rotateAbout(const vec3& v, const vec3& k, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1.f - c));
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

void MusicBoxUniverse::setupInstVao(GLuint vao, GLuint vbo, int mesh) {
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

bool MusicBoxUniverse::init(int width, int height) {
    using shaders::withCommon;
    progSolid_ = ugl::program(withCommon(shaders::SOLID_VS).c_str(), withCommon(shaders::SOLID_FS, true).c_str(), "musicbox solids");
    progShadow_ = ugl::program(withCommon(shaders::SOLID_VS).c_str(), shaders::SHADOW_FS, "musicbox shadow");
    progSprite_ = ugl::program(withCommon(shaders::SPRITE_VS).c_str(), withCommon(shaders::SPRITE_FS).c_str(), "musicbox glows");
    progLine_ = ugl::program(shaders::LINE_VS, shaders::LINE_FS, "musicbox lines");
    progVeil_ = ugl::program(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "musicbox veil");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "musicbox down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "musicbox blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "musicbox final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "musicbox shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "musicbox text");

    // ---- The meshes: a box, a cylinder (48 sides), a hemisphere (position + normal, indexed).
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
            const int S = 48;
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
        glGenVertexArrays(1, &lineVao_);
        glGenBuffers(1, &lineVbo_);
        glGenBuffers(1, &stripVbo_);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, stripVbo_);
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

    // ---- The lamp's shadow map.
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
    toothLift_.assign(TRACKS, 0.f);
    toothRing_.assign(TRACKS, 99.f);
    resize(width, height);
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

void MusicBoxUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] musicbox font init failed\n");
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

void MusicBoxUniverse::resize(int width, int height) {
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
    updateCamera(0.f);
}

MusicBoxUniverse::~MusicBoxUniverse() {
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
    for (GLuint b : {quadVbo_, spriteVbo_, lineVbo_, shapeVbo_, textVbo_, stripVbo_}) glDeleteBuffers(1, &b);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    if (shadowTex_) glDeleteTextures(1, &shadowTex_);
    for (Font* f : {&caps_, &ital_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void MusicBoxUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = 1;             // ebony and nickel
        else if (t.hasSong && t.airWeight > 0.6f) v = 3;                     // birch at night
        else if (t.hasSong && !t.minor && t.brightness > 0.55f) v = 2;       // rosewood and gilt
        else if (t.hasSong) v = 0;                                           // walnut and brass
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    if (v != variant_ || !colSet_) staticDirty_ = true;
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    Palette& P = pal_;
    for (int i = 0; i < 7; i++) {
        P.piece[i] = lin(V.piece[i]);
        P.enamel[i] = (float)V.enamel[i];
    }
    P.wood = lin(V.wood); P.woodLid = lin(V.woodLid); P.inlayA = lin(V.inlayA); P.inlayB = lin(V.inlayB); P.velvet = lin(V.velvet);
    P.brass = lin(V.brass); P.steel = lin(V.steel); P.bed = lin(V.bed); P.table = lin(V.table); P.wall = lin(V.wall); P.lamp = lin(V.lamp);
    P.amb = lin(V.amb); P.ambLow = lin(V.ambLow); P.fill = lin(V.fill); P.veil = lin(V.veil); P.ink = hex(V.ink); P.accent = hex(V.accent);
    P.porcelain = lin(V.porcelain);
    P.lampK = V.lampK; P.exposure = V.exposure; P.figure = V.figure; P.night = V.night;
    colSet_ = true;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x3B0Cull);
    // a new song played in this box: a fresh cylinder (the break's black covers the change)
    if (seconds > 0.1f) {
        cyls_.clear();
        flights_.clear();
        rowsPinned_ = 0;
        fillTune_ = playTune_ = 0;
    }
}

void MusicBoxUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

void MusicBoxUniverse::drop(float strength) {
    (void)strength;
    dropMode_ = true;
}

void MusicBoxUniverse::levelUp() { levelT_ = 0.f; }

vec2 MusicBoxUniverse::project(const vec3& p) const { return layout::project(vpB_, p, width_, height_); }

TransitionHints MusicBoxUniverse::hints() const {
    TransitionHints h;
    h.background = lerp(pal_.wood, pal_.velvet, 0.5f) * 0.6f;
    h.light = false;
    h.ink = pal_.brass * 0.8f;
    h.accent = pal_.brass;
    h.styles = TS_LUMA | TS_INK | TS_REBUILD;
    return h;
}

// =====================================================================================================
// The cylinder
// =====================================================================================================

float MusicBoxUniverse::slotAngle(int slot) const { return (float)(wrapd(phi_ - slot, SLOTS) * (TAU / SLOTS)); }

// The cylinder's center: the current one in its bearings (shifted for its tune, or coming in), the ones set aside
// on the table to the right of the box (moving there).
vec3 MusicBoxUniverse::cylCenter(int ci) const {
    const Cylinder& c = cyls_[ci];
    const vec3 home(shiftX_, CY, CZ);
    if (c.rest < 0) {
        const float e = c.inT * c.inT * (3.f - 2.f * c.inT);
        return home + UP * (14.f * (1.f - e));
    }
    const vec3 rest(BX + 5.f + c.rest * 7.5f, RC + 1.1f, CZ + 1.f);
    const float s = saturate(c.swapT);
    // lifted, carried over, set down
    const float up = smoothstepf(0.f, 0.25f, s) * (1.f - smoothstepf(0.75f, 1.f, s));
    const float k = smoothstepf(0.2f, 0.8f, s);
    return lerp(vec3(0.f, CY, CZ), rest, k) + UP * (9.f * up);
}

// The cylinder's axis (along x in the box; one set aside is turned to lie along the box's side) and its front.
vec3 MusicBoxUniverse::cylAxis(int ci, vec3* front) const {
    const Cylinder& c = cyls_[ci];
    const float yaw = c.rest < 0 ? 0.f : -0.5f * PI * smoothstepf(0.25f, 0.75f, saturate(c.swapT));
    const vec3 ax(std::cos(yaw), 0.f, -std::sin(yaw));
    if (front) *front = vec3(std::sin(yaw), 0.f, std::cos(yaw));
    return ax;
}

// A pin's head center on cylinder ci (its radial direction out), lifted off it by `lift` (cm).
vec3 MusicBoxUniverse::pinPos(int ci, const Pin& p, float lift, vec3* radial) const {
    const Cylinder& c = cyls_[ci];
    const float a = c.rest < 0 ? slotAngle(p.slot) : (float)(wrapd(c.phiAt - p.slot, SLOTS) * (TAU / SLOTS));
    vec3 fr;
    const vec3 ax = cylAxis(ci, &fr);
    const vec3 r = UP * std::sin(a) + fr * std::cos(a);
    if (radial) *radial = r;
    return cylCenter(ci) + ax * (trackX(p.track) + tuneDx(p.tune)) + r * (RC + PIN_L + lift);
}

void MusicBoxUniverse::newCylinder() {
    // the full one is set aside on the table (its turn kept), a fresh one is set in
    if (!cyls_.empty()) {
        Cylinder& old = cyls_.back();
        int rests = 0;
        for (const Cylinder& c : cyls_)
            if (c.rest >= 0) rests++;
        old.rest = rests;
        old.swapT = 0.f;
        old.phiAt = phi_;
    }
    Cylinder c;
    c.inT = cyls_.empty() ? 1.f : -0.6f; // (waits for the old one to clear the bearings)
    cyls_.push_back(c);
    fillTune_ = playTune_ = 0;
    rowsPinned_ = 0;
}

// A free slot of the current cylinder for a row landing in `landBeats`: where it will be in view on the cylinder's
// front, just over the comb, or the nearest free one to that.
int MusicBoxUniverse::reserveSlot(float landBeats) {
    if (cyls_.empty()) newCylinder();
    for (int guard = 0; guard < TUNES + 2; guard++) {
        Cylinder& c = cyls_.back();
        const double land = phi_ + landBeats * rate_; // (landBeats: in beats at the cylinder's present speed)
        const int k0 = (int)std::floor(land - 3.5);
        for (int j = 0; j < SLOTS; j++) {
            const int off = (j % 2 == 0) ? j / 2 : -(j + 1) / 2;
            int k = (k0 + off) % SLOTS;
            if (k < 0) k += SLOTS;
            if (!c.used[fillTune_][k]) {
                c.used[fillTune_][k] = true;
                return k;
            }
        }
        if (fillTune_ + 1 < TUNES) fillTune_++;
        else newCylinder();
    }
    return 0;
}

// A cleared row: ten pins reserved on the cylinder, ten cells flying there from the well.
void MusicBoxUniverse::pinRow(const std::vector<GameEvent::CellInfo>& cells, int row, int rowsInClear, bool hold, float extraDelay) {
    (void)rowsInClear;
    std::vector<GameEvent::CellInfo> cs;
    for (auto& c : cells)
        if (c.y == row && c.x >= 0 && c.x < Game::W) cs.push_back(c);
    if (cs.empty()) return;
    std::sort(cs.begin(), cs.end(), [](const GameEvent::CellInfo& a, const GameEvent::CellInfo& b) { return a.x < b.x; });
    // (the cylinder's speed now: slots per second)
    const float sps = std::max(0.5f, lastBpm_ / 60.f) * speedK_ * (1.f - 0.65f * slow_);
    const float landS = extraDelay + SLIDE_T + 2.0f;
    const int slot = reserveSlot(landS * sps);
    const int ci = (int)cyls_.size() - 1;
    Cylinder& cyl = cyls_[ci];
    cyl.rows++;
    rowsPinned_++;
    int i = 0;
    for (auto& c : cs) {
        Pin p;
        p.slot = slot;
        p.tune = fillTune_;
        p.track = c.x;
        p.type = std::clamp(c.type, 0, 6);
        p.landed = false;
        p.order = (float)cyl.rows + c.x * 0.05f;
        cyl.pins.push_back(p);
        Flight fl;
        fl.type = p.type;
        fl.b0 = layout::cellPos((float)c.x, (float)std::max(c.y, Game::HIDDEN));
        fl.delay = extraDelay + 0.04f * i;
        fl.cyl = ci;
        fl.pin = (int)cyl.pins.size() - 1;
        fl.hold = hold;
        flights_.push_back(fl);
        i++;
    }
}

// =====================================================================================================
// Game events
// =====================================================================================================

void MusicBoxUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Clear: {
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        // bottom row first
        std::sort(rows.begin(), rows.end(), [](int a, int b) { return a > b; });
        // a row arriving while the last ones are still leaving the well waits its turn
        float start = 0.f;
        for (const Flight& f : flights_)
            if (f.kind == 0 && !f.world) start = std::max(start, f.delay - f.t + 0.12f);
        for (int r = 0; r < (int)rows.size(); r++) pinRow(ev.cells, rows[r], (int)rows.size(), true, start + 0.16f * r);
        if (ev.count >= 4) quadT_ = 0.f;
        break;
    }
    case GameEvent::TopOut: overT_ = 0.f; break;
    default: break;
    }
}

// The hand-off: the cell leaves the well's space for the box's, at the same pixel and the same apparent size.
void MusicBoxUniverse::launch(Flight& fl) {
    const vec3 bp = fl.b0 + vec3(0.f, 0.f, 0.9f);
    const vec4 c0 = vpB_ * vec4(bp, 1.f);
    const vec4 ct = vpB_ * vec4(bp + vec3(0.f, CELL_H, 0.f), 1.f);
    const vec2 ndc{c0.x / c0.w, c0.y / c0.w};
    const float hpx = std::fabs(ct.y / ct.w - c0.y / c0.w) * height_ * 0.5f;
    const vec3 dir = normalize(fwdW_ + rightW_ * (ndc.x * tanW_ * aspect_) + upW_ * ((ndc.y - shiftY_) * tanW_));
    const float d0 = camD_ * 0.45f;
    fl.p0 = eyeW_ + dir * (d0 / std::max(0.2f, dot(dir, fwdW_)));
    fl.size0 = hpx * d0 * tanW_ / (height_ * 0.5f);
    fl.face0 = normalize(eyeW_ - fl.p0);
    fl.world = true;
    fl.t = 0.f;
    fl.dur = 2.0f;
}

// =====================================================================================================
// The board
// =====================================================================================================

void MusicBoxUniverse::cellInst(std::vector<Inst>& out, const vec3& c, float sc, int type, float glow, float seed, float dim) {
    Inst in{};
    const vec3 col = pal_.piece[std::clamp(type, 0, 6)] * dim;
    const float en = pal_.enamel[std::clamp(type, 0, 6)];
    in.c[0] = c.x; in.c[1] = c.y; in.c[2] = c.z - CELL_D * sc; in.c[3] = (float)M_CELL;
    in.x[0] = CELL_H * sc; in.x[3] = glow;
    in.y[1] = CELL_H * sc; in.y[3] = en;
    in.z[2] = CELL_D * sc; in.z[3] = en; // (the boss: brass on enamel)
    in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = seed;
    out.push_back(in);
}

void MusicBoxUniverse::buildBoard(const Game& g) {
    const float overDim = overT_ >= 0.f ? 1.f - 0.45f * smoothstepf(0.f, 3.f, overT_) : 1.f;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            cellInst(cells_, layout::cellPos((float)x, y - g.rowOffset(y)), 1.f, c.type, 0.f, 0.13f * x + 0.71f * y, overDim);
        }
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) cellInst(cells_, layout::cellPos(cells[i][0] + dx, cells[i][1] + dy), 1.f, p.type, 0.06f, 0.37f * i);
        // the ghost: a fine brass outline where it lands
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
            const vec3 col = lerp(pal_.piece[p.type], pal_.brass, 0.5f) * 1.3f;
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
            cellInst(cells_, vec3(center.x + ox * sc, center.y - oy * sc, 0.f), sc, type, 0.f, 0.21f * i);
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

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float MusicBoxUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) w += f.g[glyphIndex((unsigned char)s[i])].adv * k + track;
    return w - track;
}

void MusicBoxUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const {
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

void MusicBoxUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha, float haloA) {
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

void MusicBoxUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

void MusicBoxUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
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
    // the bonus gauge: a ring of SLOTS pins, set one by one in brass as it charges (none in a break)
    if (vis > 0.01f && !bare_) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const float dimk = vis * (1.f - 0.5f * dimT_);
        auto disc = [&](float x, float y, float rr, const vec3& col, float al) {
            const int n = 12;
            for (int i = 0; i < n; i++) {
                const float a0 = TAU * i / n, a1 = TAU * (i + 1) / n;
                const float v[3][6] = {{x, y, col.x, col.y, col.z, al},
                                       {x + std::cos(a0) * rr, y + std::sin(a0) * rr, col.x, col.y, col.z, al},
                                       {x + std::cos(a1) * rr, y + std::sin(a1) * rr, col.x, col.y, col.z, al}};
                for (auto& q : v) shapes_.insert(shapes_.end(), q, q + 6);
            }
        };
        const int N = SLOTS;
        const float rd = std::max(1.6f, 3.2f * s);
        for (int i = 0; i < N; i++) {
            const float a = TAU * i / N;
            const float x = c.x + std::sin(a) * r, y = c.y - std::cos(a) * r;
            const bool on = (i + 0.5f) / N <= fill;
            if (on) disc(x, y, rd * 1.25f, accent, 0.95f * dimk);
            else disc(x, y, rd * 0.6f, ink, (g.bonusReady() ? 0.6f : 0.3f) * dimk);
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
            place(ital_, h.text, base * 0.8f, base * 0.03f, base * 0.04f);
            drawText(ital_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f) * 0.8f * vis);
            continue;
        case HUD_SECTION:
            place(caps_, upper(h.text), base * 0.7f, base * 0.32f, -base * 0.02f);
            drawText(caps_, q, accent, halo, saturate(0.3f + h.alpha * 1.7f) * vis);
            continue;
        case HUD_TITLE:
            place(ital_, h.text, base * 1.08f, base * 0.0f, -base * 0.12f);
            drawText(ital_, q, ink, halo, saturate(h.alpha * 1.3f) * vis);
            continue;
        case HUD_INFO:
            place(caps_, upper(h.text), base * 0.56f, base * 0.24f);
            drawText(caps_, q, ink, halo, saturate(alpha * 1.2f) * 0.8f);
            continue;
        case HUD_LABEL:
            place(caps_, upper(h.text), base * 0.6f, base * 0.34f);
            drawText(caps_, q, accent, halo, saturate(alpha * 1.3f) * 0.9f);
            continue;
        case HUD_VALUE:
            place(caps_, h.text, base * 0.95f, base * 0.04f, -base * 0.05f);
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
            place(caps_, upper(h.text), base * 0.86f, base * 0.32f, -base * 0.1f);
            drawText(caps_, q, ink, halo, alpha, 0.65f);
            continue;
        case HUD_TOAST_SUB:
            place(ital_, h.text, base * 0.95f, base * 0.04f);
            drawText(ital_, q, accent, halo, alpha * 0.9f, 0.65f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(caps_, upper(h.text), base * 0.8f, base * 0.28f);
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
// The camera: in front of the box, a little above, the cylinder's ten tracks over the well's ten columns
// =====================================================================================================

void MusicBoxUniverse::updateCamera(float dt) {
    (void)dt;
    const layout::Rect br = layout::boardRect(width_, height_);
    const float wf = std::max(0.05f, (br.x1 - br.x0) / (float)width_);
    tanW_ = 0.3f;
    // the ten tracks (30 cm) as wide as the well
    const float span = PITCH * Game::W * 0.74f;
    camD_ = span / (2.f * tanW_ * aspect_ * wf);
    const float pitch = -34.f * PI / 180.f;
    const float yaw = 0.008f * std::sin(worldTime_ * 0.05f);
    fwdW_ = vec3(-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch));
    rightW_ = normalize(cross(fwdW_, UP));
    upW_ = cross(rightW_, fwdW_);
    targetW_ = vec3(0.f, CY + 0.5f, CZ + 1.f);
    eyeW_ = targetW_ - fwdW_ * camD_;
    // the cylinder in the upper half of the frame (an off-axis shift)
    shiftY_ = 0.17f;
    const mat4 view = mat4::lookAt(vec3(0.f), fwdW_, UP);
    mat4 shift = mat4::identity();
    shift.at(1, 3) = shiftY_;
    vpW_ = shift * mat4::perspective(2.f * std::atan(tanW_), aspect_, camD_ * 0.2f, camD_ * 6.f) * view;
}

void MusicBoxUniverse::drawInst(GLuint vao, GLuint vbo, int mesh, const std::vector<Inst>* data, size_t count) {
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
// Registry
// =====================================================================================================

const UniverseEntry& universe_musicbox() {
    static const UniverseEntry e{
        "MUSIC BOX", "a wooden cylinder music box under a warm lamp; every cleared row is pinned on its cylinder and plucks the comb on the beat, in 3D",
        [] { return std::unique_ptr<Universe>(new MusicBoxUniverse()); },
        // A calm, intimate world: suits gentle and mid-tempo songs, airy and sparse ones best.
        [](const SongLook& l) { return 0.75f + 0.25f * (1.f - l.density) + 0.15f * l.airWeight - 0.1f * l.bassWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
