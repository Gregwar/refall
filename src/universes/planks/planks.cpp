// PLANKS: wooden plank blocks in a white studio. Every cleared row pushes its planks out of the well, Jenga style,
// to build a plank village beside it, plank by plank.
//
// One world, in real 3D, BRICKS' studio made of wood: a seamless white sweep under one soft key light, seen through
// the layout's camera raised a little and looking straight ahead with a lens shift (the board's plane keeps its
// pixels while the table recedes). Everything is wood: grown rings, fibers and darker end grain, rounded arrises, a
// satin sheen. The board stays a clean Tetris board: each cell is one solid block of three thick planks laid flat,
// tight (fine seams, the end grain at the cell's sides, a small gap around it), stained all over in its piece's color
// (or painted, or a wood species), so a piece reads at a glance by its color. The well is a painted board standing on
// a plinth of crossed planks; the falling piece is held a little off it (its shadow says so), its planks a hair
// loose, and they settle when it locks. Screen-space ambient occlusion darkens the seams and the table under the
// constructions. The real plank architecture (thin planks, 1 : 3 : 15) lives in the village.
//
// The village: each side of the well lays its own builds, one after the other (build.cpp): on the left a tall tower
// with floors and a gable roof, then the left half of a track close behind the well, a pagoda far back; on the right
// a temple (porches on columns, lean-to roofs, a stepped top), the right half of the track, a train (a locomotive and
// a carriage), a long house; then more builds farther back. A cleared row: its cells tip back like dominoes, from the
// middle out, and slide back into the board; each cell's middle plank is pushed straight back into the board, Jenga
// style, and, hidden behind it, comes out from under the well's back low on the table, slides out below the HUD's
// columns and only then rises on an arc away from the camera to its place in the village, where it settles on the
// music's beat grid (half beats): nothing crosses the well or the HUD. Every plank of the village is a cleared cell
// (natural in PINE and HONEY, a Tetris keeping its paint; colored in COLOURS).
//
// The song: calm, the village stands still; mid and peak send a gentle ripple up the constructions every two beats
// (each plank lifting a hair and settling, bottom to top), and a peak sets the train shuttling along the right half
// of its track; the drop speeds it up, ripples higher and warms the light once (a slow swell). A Tetris is one big
// ripple through the whole village. Slow motion is an evening light raking in from the right. Game over: the stack's
// cells drop into the plinth, from the top down, and every plank of the village topples gracefully, from the top
// down; the next game starts a new village.
#include "universes/planks/planks.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/planks/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Nunito (SemiBold and Light), SIL Open Font License, see OFL-Nunito.txt.
const unsigned char FONT_MED[] = {
#include "universes/planks/Nunito-SemiBold.ttf.inc"
};
const unsigned char FONT_LIGHT[] = {
#include "universes/planks/Nunito-Light.ttf.inc"
};

namespace shaders = planks_shaders;
namespace pb = planks_build;
using pb::Q;
using pb::qAxis;
using pb::qMul;
using pb::qRot;
using pb::qSlerp;
using pb::qYaw;
using ugl::set1f;
using ugl::set1i;
using ugl::set3f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
vec3 mixDisplay(const vec3& a, const vec3& b, float t) { return ugl::toLinear(lerp(ugl::toSrgb(a), ugl::toSrgb(b), t)); }

constexpr float EYE_Y = 5.0f;      // the camera's height
constexpr float TABLE = pb::TABLE; // the table's plane
constexpr float WALL = -260.f;     // the backdrop
constexpr float HOLD_Z = 0.24f;    // the falling piece, held off the board
constexpr float PW = 0.2f, PT = 1.f / 15.f; // a well plank's width and thickness (its length is a cell)
constexpr float BOARD_Z = -1.06f;  // the board's front face
constexpr float SNAP = 0.35f;      // a village plank lands that far above its place and settles

// Test hooks: ZEN_PLANKS_LINES=N lays N lines' planks at once (screenshots of a long game), ZEN_GROW_RATE=R adds R
// lines a second, each one cleared like a real line (its planks pushed out of the bottom row and flown to the
// village), for a time-lapse.
int hookStart() {
    static const int n = std::getenv("ZEN_PLANKS_LINES") ? std::atoi(std::getenv("ZEN_PLANKS_LINES")) : 0;
    return n;
}
int hookRate(double t) {
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return (int)(rate * t);
}
// A cell: three thick planks laid flat, tight, one on the other (a solid block, fine seams between them).
constexpr int CELL_PLANKS = 3;
constexpr int PUSHED = 1;          // the middle one is pushed out on a clear
constexpr float SLAB_L = 0.955f, SLAB_H = 0.314f, SLAB_D = 0.94f, SLAB_STEP = 0.32f;

// The variants: the studio (table near, far, the backdrop's top, the light pool), the board, the natural wood
// (earlywood, latewood), the seven pieces (I O T S Z J L) and how much their stain covers the wood (the pieces, a
// Tetris' planks in the village, unused, the village's other planks), the grain's contrast, the accent, the light
// (strength, ambient, sheen, its direction).
struct VariantDef {
    const char* name;
    unsigned tableNear, tableFar, wallTop, pool, board;
    unsigned early, late;
    unsigned piece[7];
    float pieceK, accentK, ribK, buildK, grain;
    unsigned accent;
    float key, amb, sheen;
    float lx, ly, lz;
    unsigned keyCol;
};
const VariantDef VARIANTS[] = {
    // natural pine on white, the pieces in clear stains over the grain, a natural village
    {"PINE", 0xf4f2ee, 0xebe8e2, 0xdedad3, 0x14100a, 0x4b4641,
     0xeed6a8, 0xcf9f62,
     {0x62b2e6, 0xf4c832, 0xb24cc2, 0x55ae4c, 0xe2463a, 0x2552c4, 0xf38424},
     0.86f, 0.92f, 0.f, 0.08f, 0.75f, 0xc0603a, 1.0f, 1.0f, 0.8f, -0.3f, 0.82f, 0.48f, 0xfff6ea},
    // the colored planks of a toy set: lacquered colors over pine, a colorful village
    {"COLOURS", 0xf3f2f0, 0xe9e8e5, 0xdcdbd8, 0x10100e, 0x3e4654,
     0xefd8ab, 0xd2a266,
     {0x47a3dc, 0xf4c53a, 0x8a5cc4, 0x48a85e, 0xdf4a35, 0x2f5cad, 0xef8a2d},
     0.9f, 0.9f, 0.f, 0.6f, 0.45f, 0xdf4a35, 1.0f, 1.0f, 1.0f, -0.3f, 0.82f, 0.48f, 0xfff8f0},
    // late afternoon: honey light from low on the left, oiled pine, warm stains
    {"HONEY", 0xf2e3cf, 0xe6d0b4, 0xd6bb98, 0x2a1a08, 0x3a2b22,
     0xe8bf86, 0xbb7f42,
     {0x4e9a98, 0xeac030, 0x9a3c78, 0x6a903a, 0xb02a24, 0x2a62b0, 0xe27420},
     0.84f, 0.85f, 0.f, 0.12f, 0.9f, 0xd27a2a, 1.05f, 0.85f, 0.9f, -0.7f, 0.46f, 0.55f, 0xffe2b8},
    // a dark studio under one lamp: the pieces in natural hardwoods (maple, yellowheart, purpleheart, olive,
    // bloodwood, blue-stained ash, padauk) on a walnut plinth
    {"HARDWOOD", 0x3b342e, 0x29231e, 0x17140f, 0x5a3a18, 0x1c1916,
     0xc89a64, 0x93663c,
     {0xecdcb8, 0xe6b632, 0x9a4f96, 0x8a9a46, 0xc23a2c, 0x5a7cab, 0xdc7030},
     1.0f, 1.0f, 0.f, 0.3f, 0.9f, 0xe6b632, 1.2f, 0.95f, 1.1f, -0.3f, 0.8f, 0.5f, 0xffeedd},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

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

float easeOutBack(float t, float c1 = 1.6f) {
    const float c3 = c1 + 1.f;
    const float u = t - 1.f;
    return 1.f + c3 * u * u * u + c1 * u * u;
}
float smoother(float t) {
    t = saturate(t);
    return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
}
vec3 bezier(const vec3& a, const vec3& b, const vec3& c, const vec3& d, float t) {
    const float u = 1.f - t;
    return a * (u * u * u) + b * (3.f * u * u * t) + c * (3.f * u * t * t) + d * (t * t * t);
}
float hashf(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return (x & 0xFFFFFF) / 16777216.f;
}

// A mat4 inverse (general).
mat4 inverse(const mat4& mm) {
    const float* a = mm.m;
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
    float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    mat4 r{};
    if (std::fabs(det) < 1e-12f) return mat4::identity();
    det = 1.f / det;
    for (int i = 0; i < 16; i++) r.m[i] = inv[i] * det;
    return r;
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool PlanksUniverse::init(int width, int height) {
    using shaders::with;
    progStudio_ = ugl::program(ugl::FULLSCREEN_VS, with(shaders::STUDIO_FS, shaders::COMMON, shaders::SHADOW_GLSL).c_str(), "planks studio");
    progPlank_ = ugl::program(shaders::PLANK_VS, with(shaders::PLANK_FS, shaders::COMMON, (std::string(shaders::SHADOW_GLSL) + shaders::PLANK_COMMON).c_str()).c_str(), "planks wood");
    progShadow_ = ugl::program(shaders::PLANK_VS, shaders::SHADOW_FS, "planks shadow");
    progPre_ = ugl::program(shaders::PLANK_VS, with(shaders::PRE_FS, shaders::PLANK_COMMON).c_str(), "planks prepass");
    progPreTable_ = ugl::program(ugl::FULLSCREEN_VS, shaders::PRE_TABLE_FS, "planks prepass table");
    progSsao_ = ugl::program(ugl::FULLSCREEN_VS, shaders::SSAO_FS, "planks ssao");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "planks blur");
    progFlat_ = ugl::program(shaders::FLAT_VS, shaders::FLAT_FS, "planks flat");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "planks final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "planks shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "planks text");

    // a unit box, its six faces with their normals
    std::vector<float> box;
    const float N[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    for (auto& n : N) {
        const vec3 nv(n[0], n[1], n[2]);
        const vec3 u = std::fabs(nv.y) > 0.5f ? vec3(1, 0, 0) : normalize(cross(vec3(0, 1, 0), nv));
        const vec3 v = cross(nv, u);
        const float c[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
        for (auto& k : c) {
            const vec3 p = nv * 0.5f + u * (0.5f * k[0]) + v * (0.5f * k[1]);
            const float vtx[6] = {p.x, p.y, p.z, nv.x, nv.y, nv.z};
            box.insert(box.end(), vtx, vtx + 6);
        }
    }
    glGenVertexArrays(1, &boxVao_);
    glGenBuffers(1, &boxVbo_);
    glGenBuffers(1, &instVbo_);
    glBindVertexArray(boxVao_);
    glBindBuffer(GL_ARRAY_BUFFER, boxVbo_);
    glBufferData(GL_ARRAY_BUFFER, box.size() * sizeof(float), box.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    for (int i = 0; i < 5; i++) {
        glEnableVertexAttribArray(2 + i);
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(Inst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(2 + i, 1);
    }

    glGenVertexArrays(1, &flatVao_);
    glGenBuffers(1, &flatVbo_);
    glBindVertexArray(flatVao_);
    glBindBuffer(GL_ARRAY_BUFFER, flatVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void*)(sizeof(float) * 3));
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

    // two shadow maps for the one key light: the whole village, and the well up close (the stack on its board)
    for (int i = 0; i < 2; i++) {
        const int S = i == 0 ? 4096 : 2048;
        glGenTextures(1, &shadowTex_[i]);
        glBindTexture(GL_TEXTURE_2D, shadowTex_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, S, S, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glGenFramebuffers(1, &shadowFbo_[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_[i], 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLint maxS = 4;
    glGetIntegerv(GL_MAX_SAMPLES, &maxS);
    samples_ = std::max(1, std::min(8, (int)maxS));

    buildFont(fontMed_, FONT_MED);
    buildFont(fontLight_, FONT_LIGHT);
    resetVillage();
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void PlanksUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] planks font init failed\n");
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

// The layout's reference camera raised to EYE_Y and looking straight ahead, its image shifted back down (a lens
// shift): the board's plane lands exactly on the reference camera's pixels and the table recedes in perspective.
void PlanksUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera ref = layout::camera(width_, height_);
    const float aspect = (float)width_ / height_;
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    const float focal = 1.f / std::tan(half);
    eye_ = vec3(ref.target.x, EYE_Y, ref.target.z + ref.dist);
    view_ = mat4::lookAt(eye_, eye_ + vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f));
    mat4 shift = mat4::identity();
    shift.at(1, 3) = focal * (EYE_Y - ref.target.y) / ref.dist;
    proj_ = shift * mat4::perspective(2.f * half, aspect, 0.5f, 1200.f);
    vp_ = proj_ * view_;
    invVp_ = inverse(vp_);

    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false);
    for (auto& t : aoT_) t.create(width_, height_, GL_R8, GL_RED, GL_UNSIGNED_BYTE, false);
    if (msFbo_) {
        glDeleteFramebuffers(1, &msFbo_);
        glDeleteRenderbuffers(1, &msColor_);
        glDeleteRenderbuffers(1, &msDepth_);
    }
    glGenFramebuffers(1, &msFbo_);
    glGenRenderbuffers(1, &msColor_);
    glGenRenderbuffers(1, &msDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_RGBA16F, width_, height_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_DEPTH_COMPONENT24, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);

    // the prepass: view-space positions and normals (for the occlusion)
    if (preFbo_) {
        glDeleteFramebuffers(1, &preFbo_);
        glDeleteTextures(1, &prePos_);
        glDeleteTextures(1, &preNrm_);
        glDeleteRenderbuffers(1, &preDepth_);
    }
    glGenFramebuffers(1, &preFbo_);
    glGenTextures(1, &prePos_);
    glGenTextures(1, &preNrm_);
    glGenRenderbuffers(1, &preDepth_);
    auto tex = [&](GLuint t, GLenum internal, GLenum type) {
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, internal, width_, height_, 0, GL_RGBA, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    tex(prePos_, GL_RGBA32F, GL_FLOAT);
    tex(preNrm_, GL_RGBA16F, GL_HALF_FLOAT);
    glBindRenderbuffer(GL_RENDERBUFFER, preDepth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, preFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, prePos_, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, preNrm_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, preDepth_);
    const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, bufs);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

PlanksUniverse::~PlanksUniverse() {
    sceneT_.destroy();
    for (auto& t : aoT_) t.destroy();
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (preFbo_) glDeleteFramebuffers(1, &preFbo_);
    if (preDepth_) glDeleteRenderbuffers(1, &preDepth_);
    for (GLuint t : {prePos_, preNrm_, shadowTex_[0], shadowTex_[1]})
        if (t) glDeleteTextures(1, &t);
    for (GLuint f : shadowFbo_)
        if (f) glDeleteFramebuffers(1, &f);
    for (GLuint p : {progStudio_, progPlank_, progShadow_, progPre_, progPreTable_, progSsao_, progBlur_, progFlat_,
                     progFinal_, progShape_, progText_})
        glDeleteProgram(p);
    for (GLuint v : {boxVao_, flatVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {boxVbo_, instVbo_, flatVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontMed_, &fontLight_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

vec3 PlanksUniverse::pieceTint(int t) const { return lin(VARIANTS[variant_].piece[std::clamp(t, 0, 6)]); }

vec3 PlanksUniverse::backdrop() const {
    const VariantDef& V = VARIANTS[variant_];
    return mixDisplay(lin(V.tableFar), lin(V.wallTop), 0.3f);
}

void PlanksUniverse::setSong(const SongLook& t, float seconds) {
    (void)seconds;
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 3;                 // hardwood
        else if (t.hasSong && t.brightness > 0.6f) v = h < 0.5f ? 0 : 1;        // pine, colours
        else if (t.hasSong && t.bpm < 100.f) v = 2;                             // honey
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
}

void PlanksUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)impact;
    level = std::clamp(level, 0, 2);
    energy_ = energy;
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
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
}

// The drop: the train sets off fast, the village ripples higher, the light warms once.
void PlanksUniverse::drop(float strength) {
    dropMode_ = true;
    dropRelease_ = 1.f;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
    rippleT_ = 0.f;
    rippleA_ = 0.16f;
}

void PlanksUniverse::refresh(float strength) {
    if (strength > 0.3f && lv_ > 0.5f && rippleT_ > 1.5f) {
        rippleT_ = 0.f;
        rippleA_ = 0.08f;
    }
}

void PlanksUniverse::levelUp() {
    rippleT_ = 0.f;
    rippleA_ = 0.1f;
}

vec2 PlanksUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints PlanksUniverse::hints() const {
    TransitionHints h;
    const VariantDef& V = VARIANTS[variant_];
    h.background = backdrop();
    h.light = ugl::luma(h.background) > 0.25f;
    h.ink = h.light ? lin(0x3a2c20) : lin(0xf0e2c8);
    h.accent = lin(V.accent);
    h.styles = TS_REBUILD | TS_TILES | TS_LUMA;
    return h;
}

// =====================================================================================================
// A cell: a solid block of three planks
// =====================================================================================================

int PlanksUniverse::cellPlanks() const { return CELL_PLANKS; }

// The k-th plank of a unit cell (centered on 0): three thick planks laid flat along the row, bottom to top, tight
// (fine seams between them and a small gap around the cell): their wide faces to the front, their end grain at the
// cell's sides.
vec3 PlanksUniverse::cellPlankPos(int k, vec3* size, Q* q) const {
    *size = vec3(SLAB_L, SLAB_H, SLAB_D);
    *q = Q{};
    return vec3(0.f, (k - 1) * SLAB_STEP, 0.f);
}

void PlanksUniverse::push(const vec3& c, const vec3& size, const Q& q, const vec3& tint, float tintK, float seed,
                          float stack, float grain, float mat, float glow, float fog) {
    inst_.push_back({{c.x, c.y, c.z, seed}, {size.x, size.y, size.z, mat}, {tint.x, tint.y, tint.z, tintK},
                     {q.x, q.y, q.z, q.w}, {stack, glow, fog, grain}});
}

void PlanksUniverse::pushCell(const vec3& c, float sc, const Q& q, int type, float seed, float stack, float gap,
                              int skipPlank, float glow) {
    const VariantDef& V = VARIANTS[variant_];
    const vec3 tint = pieceTint(type);
    const int n = cellPlanks();
    for (int k = 0; k < n; k++) {
        if (k == skipPlank) continue;
        vec3 size;
        Q qk;
        vec3 p = cellPlankPos(k, &size, &qk);
        // the courses loose (a piece held in the hand), settling when it locks
        p.y += gap * (float)(k - 1) * 3.f;
        push(c + qRot(q, p * sc), size * sc, qMul(q, qk), tint, V.pieceK, hashf((uint32_t)(seed * 1e6f) + k * 7919u),
             stack, V.grain, 0.f, glow);
    }
}

// =====================================================================================================
// The well
// =====================================================================================================

void PlanksUniverse::syncCells(const Game& g) {
    // a cell that appeared without a lock (a scene review's filled board, a universe coming in mid-game): its seed
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const bool full = g.cell(x, y).type >= 0;
            if (full && seeds_[y][x] == 0.f) { seeds_[y][x] = 0.001f + hashf((uint32_t)(x * 131 + y * 977 + lockCount_ * 7)); settle_[y][x] = 9.f; }
            if (!full) { seeds_[y][x] = 0.f; press_[y][x] = pressV_[y][x] = 0.f; }
        }
}

void PlanksUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        // the piece's courses settle onto the board (the same seeds as while it fell)
        int minx = 99, miny = 99;
        for (auto& c : ev.cells) { minx = std::min(minx, c.x); miny = std::min(miny, c.y); }
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            seeds_[c.y][c.x] = 0.001f + hashf((uint32_t)(lockCount_ * 7717 + (c.x - minx) * 31 + (c.y - miny) * 131));
            press_[c.y][c.x] = HOLD_Z;
            pressV_[c.y][c.x] = -1.2f;
            settle_[c.y][c.x] = 0.f;
        }
        lockCount_++;
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        const bool tetris = ev.count >= 4;
        int rowK = 0;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (!rows[y]) continue;
            for (auto& c : ev.cells) {
                if (c.y != y) continue;
                const float seed = seeds_[y][c.x] > 0.f ? seeds_[y][c.x] : 0.5f;
                // the cell tips back into the board, a domino wave from the middle out
                clearing_.push_back({c.x, y, std::clamp(c.type, 0, 6), seed, 0.f, 0.035f * std::fabs(c.x - 4.5f) + 0.03f * rowK});
                // its middle plank is pushed out sideways and flies to the village
                const int side = c.x < 5 ? 0 : 1;
                ensureProgram(side, reserved_[side] + 1);
                Flight f;
                f.side = side;
                f.idx = reserved_[side]++;
                slots_[side][f.idx].state = 1;
                f.type = std::clamp(c.type, 0, 6);
                f.painted = tetris;
                f.seed = hashf((uint32_t)(seed * 1e6f) + (uint32_t)PUSHED * 7919u);
                vec3 sz;
                Q qq;
                f.p0 = layout::cellPos((float)c.x, (float)y) + vec3(0.f, 0.f, -0.5f) + cellPlankPos(PUSHED, &sz, &qq);
                const float sg = side ? 1.f : -1.f;
                // pushed straight back into the board (it vanishes in it), then, hidden behind the board, down to the
                // table: it slides out from under the well's back, low, under the HUD's columns, and only then rises
                f.lane = -9.62f + 0.2f * (float)rowK;
                f.slideDur = 0.2f + 0.012f * std::fabs(c.x - 4.5f);
                f.runFrom = vec3(f.p0.x, f.lane, -2.0f);
                f.slideTo = vec3(sg * 11.5f, f.lane, -2.7f);
                f.runDur = 0.06f + (11.5f - std::fabs(f.p0.x)) / 20.f;
                // it lands on the beat grid (half beats), after the ones before it on its side
                const float launch = time_ + f.slideDur + f.runDur;
                const float minFlight = (tetris ? 1.1f : 0.85f) + 0.03f * rowK;
                float arrive = std::max(launch + minFlight, lastArrive_[side] + beatLen_ * 0.5f);
                const float backlog = arrive - launch;
                if (backlog > 2.4f) arrive = std::max(launch + minFlight, lastArrive_[side] + beatLen_ * 0.25f);
                if (hasBeat_) {
                    const double b = beat_ + (arrive - time_) / beatLen_;
                    const double bq = std::ceil(b * (backlog > 2.4f ? 4.0 : 2.0) - 1e-3) / (backlog > 2.4f ? 4.0 : 2.0);
                    arrive = time_ + (float)((bq - beat_) * beatLen_);
                }
                lastArrive_[side] = arrive;
                f.arcDur = arrive - launch;
                flights_.push_back(f);
            }
            rowK++;
        }
        // the seeds follow their rows down
        {
            float ns[Game::H][Game::W] = {}, np[Game::H][Game::W] = {}, nv[Game::H][Game::W] = {}, nt[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(ns[dst], seeds_[y], sizeof(seeds_[y]));
                std::memcpy(np[dst], press_[y], sizeof(press_[y]));
                std::memcpy(nv[dst], pressV_[y], sizeof(pressV_[y]));
                std::memcpy(nt[dst], settle_[y], sizeof(settle_[y]));
                dst--;
            }
            std::memcpy(seeds_, ns, sizeof(seeds_));
            std::memcpy(press_, np, sizeof(press_));
            std::memcpy(pressV_, nv, sizeof(pressV_));
            std::memcpy(settle_, nt, sizeof(settle_));
        }
        if (tetris) {
            rippleT_ = -0.6f; // after the planks have landed... a wave runs up the whole village
            rippleA_ = 0.2f;
            glowAll_ = 0.1f;
        }
        break;
    }
    case GameEvent::TopOut: {
        // the stack's cells drop into the plinth, from the top down; the village topples
        overT_ = 0.f;
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            Fall f;
            f.p = layout::cellPos((float)c.x, (float)c.y) + vec3(0.f, 0.f, -0.5f);
            f.v = vec3(0.f, 0.f, 0.f);
            f.q0 = Q{};
            f.q1 = qAxis(vec3(1, 0, 0), -rng_.range(0.15f, 0.45f));
            f.t = 0.f;
            f.delay = 0.25f + 0.045f * (c.y - top) + rng_.range(0.f, 0.06f);
            f.seed = seeds_[c.y][c.x] > 0.f ? seeds_[c.y][c.x] : rng_.uniform();
            f.type = std::clamp(c.type, 0, 6);
            f.sc = 1.f;
            f.stackCell = true;
            falls_.push_back(f);
        }
        toppleVillage();
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The village
// =====================================================================================================

void PlanksUniverse::resetVillage() {
    for (int s = 0; s < 2; s++) {
        prog_[s] = pb::program(s);
        slots_[s].assign(prog_[s].size(), Slot{});
        reserved_[s] = 0;
        extras_[s] = 0;
        lastArrive_[s] = time_;
    }
    flights_.clear();
    trainS_ = pb::trainStation();
    trainV_ = 0.f;
    trainDir_ = 1.f;
    toppled_ = false;
    // the test hook's lines, laid at once
    for (int l = 0; l < hookStart(); l++)
        for (int x = 0; x < Game::W; x++) placeNow(x < 5 ? 0 : 1, rng_.irange(0, 6));
}

void PlanksUniverse::ensureProgram(int side, int n) {
    // beyond the authored builds: more builds farther back
    while ((int)prog_[side].size() < n) {
        const std::vector<pb::PDef> more = pb::extra(side, extras_[side]++);
        prog_[side].insert(prog_[side].end(), more.begin(), more.end());
        slots_[side].resize(prog_[side].size());
    }
}

// How much of its piece's color a village plank keeps: a little (the natural variants), all of it (the colored
// planks), and a Tetris' planks keep their paint.
float PlanksUniverse::slotTintK(bool painted) const {
    const VariantDef& V = VARIANTS[variant_];
    return painted ? std::max(V.buildK, V.accentK) : V.buildK;
}

// A plank of the village laid at once (the test hook's lines, a universe coming in mid-game).
void PlanksUniverse::placeNow(int side, int type) {
    ensureProgram(side, reserved_[side] + 1);
    Slot& s = slots_[side][reserved_[side]];
    s.state = 2;
    s.t = 9.f;
    s.type = type;
    s.painted = rng_.chance(0.06f);
    s.seed = rng_.uniform();
    reserved_[side]++;
}

bool PlanksUniverse::trainBuilt() const {
    for (size_t i = 0; i < prog_[1].size(); i++)
        if (prog_[1][i].group && slots_[1][i].state != 2) return false;
    return true;
}

// A plank's pose now: the train's planks ride their car along the track; the landing's settle and the ripples.
pb::PDef PlanksUniverse::placed(int side, int i, bool withMotion) const {
    pb::PDef d = prog_[side][i];
    if (d.group) {
        const float s = d.group == 1 ? trainS_ : trainS_ - pb::CAR_GAP;
        vec3 tan;
        const vec3 p = pb::trackPoint(s, &tan);
        const vec3 X = tan, Y(0.f, 1.f, 0.f), Z = cross(X, Y);
        const Q q = pb::qBasis(X, Y, Z);
        d.c = p + vec3(0.f, pb::TC * 2.f, 0.f) + qRot(q, d.c);
        d.q = qMul(q, d.q);
        d.h += 0.3f;
    }
    if (withMotion) {
        const Slot& sl = slots_[side][i];
        if (sl.t < 0.45f) d.c.y += SNAP * (1.f - easeOutBack(saturate(sl.t / 0.45f), 1.2f));
        if (rippleT_ < 4.f && rippleA_ > 0.f) {
            const float u = (rippleT_ - d.h * 0.07f) / 0.42f;
            if (u > 0.f && u < 1.f) {
                const float b = std::sin(PI * u);
                d.c.y += rippleA_ * b * b;
            }
        }
    }
    return d;
}

// Game over: every plank of the village topples, from the top down, onto the table around its build.
void PlanksUniverse::toppleVillage() {
    if (toppled_) return;
    toppled_ = true;
    float maxH = 0.f;
    for (int s = 0; s < 2; s++)
        for (size_t i = 0; i < prog_[s].size(); i++)
            if (slots_[s][i].state == 2) maxH = std::max(maxH, prog_[s][i].h);
    for (int s = 0; s < 2; s++) {
        for (size_t i = 0; i < prog_[s].size(); i++) {
            Slot& sl = slots_[s][i];
            if (sl.state != 2) continue;
            const pb::PDef d = placed(s, (int)i, false);
            Topple t;
            t.p0 = d.c;
            t.q0 = d.q;
            // away from the well and from the camera, more for the higher planks
            const float h = std::max(0.f, d.c.y - TABLE);
            const float out = 0.4f + 0.18f * h + rng_.range(0.f, 0.8f);
            const float a = rng_.range(-0.9f, 0.9f);
            const vec3 dir = normalize(vec3((s ? 1.f : -1.f) * std::cos(a) * 0.7f, 0.f, -0.6f - std::fabs(std::sin(a)) * 0.6f));
            t.p1 = vec3(d.c.x, TABLE + pb::TC * 0.5f + rng_.range(0.f, 0.12f), d.c.z) + dir * out;
            t.q1 = qMul(qYaw(rng_.range(-PI, PI)), qAxis(vec3(1, 0, 0), PI * 0.5f));
            t.type = sl.type;
            t.painted = sl.painted;
            t.seed = sl.seed;
            t.delay = 0.5f + (maxH - d.h) * 0.07f + rng_.range(0.f, 0.25f);
            t.dur = std::clamp(std::sqrt(2.f * h / 14.f) + 0.25f, 0.35f, 1.4f);
            t.t = 0.f;
            topple_.push_back(t);
            sl.state = 0;
        }
    }
    flights_.clear();
    toppleFade_ = 0.f;
}

void PlanksUniverse::updateFlights(float dt) {
    for (Flight& f : flights_) {
        f.t += dt;
        if (f.phase == 0 && f.t >= f.slideDur) { f.phase = 1; f.t -= f.slideDur; }
        if (f.phase == 1 && f.t >= f.runDur) { f.phase = 2; f.t -= f.runDur; }
        if (f.phase == 2 && f.t >= f.arcDur) {
            f.phase = 3;
            if (f.idx < (int)slots_[f.side].size()) {
                Slot& s = slots_[f.side][f.idx];
                s.state = 2;
                s.t = 0.f;
                s.type = f.type;
                s.painted = f.painted;
                s.seed = f.seed;
            }
        }
    }
    flights_.erase(std::remove_if(flights_.begin(), flights_.end(), [](const Flight& f) { return f.phase == 3; }), flights_.end());
    for (int s = 0; s < 2; s++)
        for (Slot& sl : slots_[s])
            if (sl.state == 2 && sl.t < 9.f) sl.t += dt;
    for (ClearCell& c : clearing_) c.t += dt;
    clearing_.erase(std::remove_if(clearing_.begin(), clearing_.end(), [](const ClearCell& c) { return c.t > c.delay + 0.45f; }),
                    clearing_.end());
    for (Fall& f : falls_) f.t += dt;
    falls_.erase(std::remove_if(falls_.begin(), falls_.end(), [](const Fall& f) { return f.t > f.delay + 1.6f; }), falls_.end());
    for (Topple& t : topple_) t.t += dt;
}

void PlanksUniverse::buildVillage() {
    const VariantDef& V = VARIANTS[variant_];
    const vec3 size(pb::LC, pb::WC, pb::TC);
    for (int s = 0; s < 2; s++)
        for (size_t i = 0; i < prog_[s].size(); i++) {
            const Slot& sl = slots_[s][i];
            if (sl.state != 2) continue;
            const pb::PDef d = placed(s, (int)i, true);
            const float haze = saturate((-d.c.z - 52.f) / 150.f) * 0.75f;
            push(d.c, size, d.q, pieceTint(sl.type), slotTintK(sl.painted), sl.seed, 0.f, V.grain, 0.f, glowAll_ * 0.5f, haze);
        }
    // the planks flying from the well: pushed back into the board, out from under it behind the well (low, under the
    // HUD's columns), then an arc away from the camera to their place
    for (const Flight& f : flights_) {
        if (f.phase == 0) {
            if (bare_) continue;
            const float u = saturate(f.t / f.slideDur);
            const vec3 pos = f.p0 + vec3(0.f, 0.f, -1.6f * u * u);
            push(pos, vec3(SLAB_L, SLAB_H, SLAB_D), Q{}, pieceTint(f.type), V.pieceK, f.seed, -1.f, V.grain);
            continue;
        }
        vec3 pos;
        Q q;
        float sc = 1.25f, k = 0.f;
        if (f.phase == 1) {
            const float u = saturate(f.t / f.runDur);
            pos = lerp(f.runFrom, f.slideTo, u * (2.f - u) * 0.5f + u * 0.5f);
        } else {
            const pb::PDef d = placed(f.side, f.idx, false);
            const vec3 tgt = d.c + vec3(0.f, SNAP, 0.f);
            const float u = saturate(f.t / std::max(0.05f, f.arcDur));
            const float e = 0.45f * u + (3.f - 2.f * 0.45f) * u * u + (0.45f - 2.f) * u * u * u;
            const float sg = f.side ? 1.f : -1.f;
            // out along the table first (below the columns), then up and back to its place: z only ever decreases
            const float zMid = std::min(f.slideTo.z, 0.5f * (f.slideTo.z + tgt.z));
            const vec3 c1(f.slideTo.x + sg * 5.5f, f.slideTo.y + 0.2f, zMid);
            const vec3 c2(tgt.x, std::max(tgt.y, f.slideTo.y) + 2.6f, tgt.z);
            pos = bezier(f.slideTo, c1, c2, tgt, e);
            k = smoothstepf(0.05f, 0.9f, u);
            q = qSlerp(Q{}, d.q, smoother(u));
            sc = lerpf(1.25f, pb::LC, k);
        }
        push(pos, vec3(1.f, PW, PT) * sc, q, pieceTint(f.type), lerpf(V.pieceK, slotTintK(f.painted), k), f.seed, 0.f, V.grain);
    }
    // game over: the village's planks toppling and lying on the table (fading when a new game starts)
    for (const Topple& t : topple_) {
        const float u = saturate((t.t - t.delay) / t.dur);
        vec3 p = lerp(t.p0, t.p1, smoothstepf(0.f, 1.f, u));
        p.y = lerpf(t.p0.y, t.p1.y, u * u);
        const Q q = qSlerp(t.q0, t.q1, smoother(u));
        const float haze = saturate((-p.z - 52.f) / 150.f) * 0.75f;
        push(p, size, q, pieceTint(t.type), slotTintK(t.painted), t.seed, 0.f, V.grain, 0.f, 0.f, std::max(haze, toppleFade_));
    }
}

void PlanksUniverse::buildBoard(const Game& g) {
    if (bare_) {
        flat_.clear();
        previewStart_ = previewEnd_ = inst_.size();
        return;
    }
    const VariantDef& V = VARIANTS[variant_];
    const Q id;
    const vec3 natural(1.f, 1.f, 1.f);
    // the board standing behind the well, on the table
    const float boardTop = 10.35f;
    push(vec3(0.f, 0.5f * (TABLE + boardTop), BOARD_Z - 0.14f), vec3(10.5f, boardTop - TABLE, 0.28f), id, lin(V.board), 1.f, 0.37f,
         0.f, 1.f, 1.f);
    // the plinth: two courses of planks on edge under the well (front and back rows, planks across between)
    {
        const float y0 = TABLE;
        for (int c = 0; c < 2; c++) {
            const float y = y0 + c * PW + PW * 0.5f;
            if (c == 0) {
                for (int i = 0; i <= 10; i++)
                    push(vec3(-5.f + i * 1.f, y, -0.5f), vec3(1.f, PW, PT), qYaw(PI * 0.5f), natural, 0.f,
                         hashf(900u + i) , 0.f, V.grain);
            } else {
                for (int i = 0; i < 10; i++)
                    for (int s = 0; s < 2; s++)
                        push(vec3(-4.5f + i, y, s ? -1.f + PT * 0.5f : -PT * 0.5f), vec3(1.f, PW, PT), id, natural, 0.f,
                             hashf(1000u + i * 2 + s), 0.f, V.grain);
            }
        }
    }

    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const float off = g.rowOffset(y);
            const vec3 p = layout::cellPos((float)x, y - off) + vec3(0.f, 0.f, -0.5f + press_[y][x]);
            const float st = settle_[y][x];
            const float gap = st < 0.25f ? 0.0035f * (1.f - easeOutBack(saturate(st / 0.25f), 2.2f)) : 0.f;
            pushCell(p, 1.f, id, c.type, seeds_[y][x] > 0.f ? seeds_[y][x] : 0.3f, 1.f, gap);
        }
    // the cleared rows: the cells tipping back into the board (their middle plank already gone)
    for (const ClearCell& cc : clearing_) {
        const float u = saturate((cc.t - cc.delay) / 0.4f);
        const vec3 base = layout::cellPos((float)cc.x, (float)cc.y) + vec3(0.f, 0.f, -0.5f);
        // tips back about its foot's back edge, and slides into the board
        const vec3 pivot = base + vec3(0.f, -0.5f, -0.5f);
        const Q q = qAxis(vec3(1, 0, 0), -0.5f * smoothstepf(0.f, 0.7f, u));
        const vec3 c = pivot + qRot(q, base - pivot) + vec3(0.f, 0.f, -1.25f * u * u);
        pushCell(c, 1.f, q, cc.type, cc.seed, -1.f, 0.f, PUSHED);
    }
    // game over: the stack's cells drop into the plinth
    for (const Fall& f : falls_) {
        const float t = std::max(0.f, f.t - f.delay);
        const vec3 p = f.p + vec3(0.f, -0.5f * 22.f * t * t, -0.3f * t);
        const Q q = qSlerp(f.q0, f.q1, saturate(t * 2.f));
        pushCell(p, f.sc, q, f.type, f.seed, -2.f, 0.f);
    }

    // the falling piece: its courses a little loose, held off the board
    flat_.clear();
    if (g.hasPiece() && !g.over()) {
        const Piece& pc = g.piece();
        int cells[4][2];
        g.pieceCells(pc, cells);
        int minx = 99, miny = 99;
        for (auto& c : cells) { minx = std::min(minx, c[0]); miny = std::min(miny, c[1]); }
        const float dx = g.pieceVisualX() - pc.x, dy = g.pieceVisualY() - pc.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy) + vec3(0.f, 0.f, -0.5f + HOLD_Z);
            const float seed = 0.001f + hashf((uint32_t)(lockCount_ * 7717 + (c[0] - minx) * 31 + (c[1] - miny) * 131));
            pushCell(w, 1.f, id, pc.type, seed, 1.f, 0.0035f);
        }
        // the ghost: a fine outline on the board where it will land
        Piece gp = pc;
        gp.y = g.ghostY();
        if (gp.y > pc.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            auto inG = [&](int x, int y) {
                for (auto& c : gc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            };
            const vec3 bc = lin(V.board);
            const bool lightBoard = ugl::luma(bc) > 0.2f;
            const vec3 ink = lightBoard ? mixDisplay(bc, lin(0x1a1a1e), 0.55f) : mixDisplay(bc, lin(0xf4ead8), 0.42f);
            const float a = 0.85f, wdt = 0.05f, in = 0.07f;
            auto seg = [&](float x0, float y0, float x1, float y1) {
                const float ex = (x1 - x0), ey = (y1 - y0);
                const float l = std::sqrt(ex * ex + ey * ey);
                const float nx = -ey / l * wdt * 0.5f, ny = ex / l * wdt * 0.5f;
                const float zz = BOARD_Z + 0.005f;
                const float v[6][3] = {{x0 - nx, y0 - ny, zz}, {x1 - nx, y1 - ny, zz}, {x1 + nx, y1 + ny, zz},
                                       {x0 - nx, y0 - ny, zz}, {x1 + nx, y1 + ny, zz}, {x0 + nx, y0 + ny, zz}};
                for (auto& q : v) {
                    const float vv[7] = {q[0], q[1], q[2], ink.x, ink.y, ink.z, a};
                    flat_.insert(flat_.end(), vv, vv + 7);
                }
            };
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                const float l = w.x - 0.5f + in, r = w.x + 0.5f - in, t = w.y + 0.5f - in, b = w.y - 0.5f + in;
                const bool L = inG(c[0] - 1, c[1]), R = inG(c[0] + 1, c[1]), U = inG(c[0], c[1] - 1), D = inG(c[0], c[1] + 1);
                const float l2 = L ? w.x - 0.5f : l, r2 = R ? w.x + 0.5f : r, t2 = U ? w.y + 0.5f : t, b2 = D ? w.y - 0.5f : b;
                if (!L) seg(l, b2, l, t2);
                if (!R) seg(r, b2, r, t2);
                if (!U) seg(l2, t, r2, t);
                if (!D) seg(l2, b, r2, b);
            }
        }
    }

    // the previews: small planked pieces (no shadow)
    previewStart_ = inst_.size();
    auto preview = [&](int type, vec2 center, float sc, float dim) {
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
            pushCell(vec3(px, py, -0.5f * sc), sc, id, type, 0.11f + 0.17f * i + 0.05f * type, 0.f, 0.f, -1, 0.f);
            if (dim > 0.f)
                for (size_t k = inst_.size() - cellPlanks(); k < inst_.size(); k++) inst_[k].e[2] = dim;
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.55f : 0.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 0.f);
    }
    previewEnd_ = inst_.size();
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float PlanksUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv * k + track;
    }
    return w - track;
}

void PlanksUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                               std::vector<float>& out) const {
    if (!f.ok) return;
    const float k = size / f.px;
    const float baseline = y + f.capH * k;
    float pen = x;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        const Glyph& g = f.g[ch - 32];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            const float v[6][4] = {{x0, y0, g.x0, g.y0}, {x1, y0, g.x1, g.y0}, {x1, y1, g.x1, g.y1},
                                   {x0, y0, g.x0, g.y0}, {x1, y1, g.x1, g.y1}, {x0, y1, g.x0, g.y1}};
            for (auto& q : v) out.insert(out.end(), q, q + 4);
        }
        pen += g.adv * k + track;
    }
}

void PlanksUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    ugl::set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", 0.5f);
    set1f(progText_, "uAlpha", alpha * (1.f - smoothstepf(0.4f, 0.9f, bridge_.amount)));
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void PlanksUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    ugl::set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// The lettering in a warm ink (dark walnut on the pale studios, cream on the dark one); the bonus gauge a ring of
// twelve little planks laid like the spokes of a wheel, the charged ones in the accent.
void PlanksUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    const VariantDef& V = VARIANTS[variant_];
    const vec3 bg = backdrop();
    const bool pale = ugl::luma(bg) > 0.25f;
    const vec3 ink = ugl::toSrgb(pale ? lin(0x3a2e24) : lin(0xf2e6d2)), halo = ugl::toSrgb(pale ? lin(0xf6f2ea) : lin(0x100d0b));
    const vec3 accent = ugl::toSrgb(pale ? mixDisplay(lin(V.accent), lin(0x000000), 0.12f) : lin(V.accent));
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);

    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float q[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                   {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : q) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }

    if (vis > 0.01f && !bare_) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = Game::BONUS_LINES;
        auto plank = [&](float cx, float cy, float ang, float len, float wid, const vec3& col, float al) {
            const float ux = std::sin(ang), uy = -std::cos(ang), vx = -uy, vy = ux;
            const float hl = len * 0.5f, hw = wid * 0.5f;
            const float px[4] = {cx - ux * hl - vx * hw, cx + ux * hl - vx * hw, cx + ux * hl + vx * hw, cx - ux * hl + vx * hw};
            const float py[4] = {cy - uy * hl - vy * hw, cy + uy * hl - vy * hw, cy + uy * hl + vy * hw, cy - uy * hl + vy * hw};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {px[k], py[k], col.x, col.y, col.z, al};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
        };
        for (int i = 0; i < N; i++) {
            const float a = TAU * i / N;
            const float cx = c.x + std::sin(a) * r * 0.78f, cy = c.y - std::cos(a) * r * 0.78f;
            const float part = saturate(fill * N - i);
            const float al = (g.bonusReady() ? 0.8f : 0.45f) * vis * (1.f - 0.5f * dimT_);
            plank(cx, cy, a, r * 0.5f, r * 0.13f, ink, al * (1.f - part) * 0.55f);
            plank(cx, cy, a, r * 0.5f, r * 0.13f, accent, part * 0.95f * vis * (1.f - 0.5f * dimT_));
        }
        flushShapes();
    }

    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        auto upper = [&](std::string t) {
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            return t;
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(fontMed_, upper(h.text), base * 0.7f, base * 0.2f, base * 0.1f);
            drawText(fontMed_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f));
            continue;
        case HUD_SECTION:
            place(fontMed_, upper(h.text), base * 0.88f, base * 0.18f);
            drawText(fontMed_, q, accent, halo, saturate(0.25f + h.alpha * 1.7f));
            continue;
        case HUD_TITLE:
            place(fontLight_, h.text, base * 1.12f, 0.f, -base * 0.1f);
            drawText(fontLight_, q, ink, halo, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontLight_, h.text, base * 0.72f, base * 0.06f);
            drawText(fontLight_, q, ink, halo, saturate(alpha * 1.2f) * 0.75f);
            continue;
        case HUD_LABEL:
            place(fontMed_, upper(h.text), base * 0.6f, base * 0.26f, base * 0.06f);
            drawText(fontMed_, q, ink, halo, saturate(alpha * 1.2f) * 0.7f);
            continue;
        case HUD_VALUE:
            place(fontLight_, h.text, base * 1.02f, base * 0.02f);
            drawText(fontLight_, q, ink, halo, alpha);
            continue;
        case HUD_NOTICE:
            place(fontMed_, h.text, base * 0.95f, base * 0.04f);
            drawText(fontMed_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontMed_, h.text, base * 0.85f, 0.f);
            drawText(fontMed_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontMed_, upper(h.text), base * 1.1f, base * 0.16f, -base * 0.1f);
            drawText(fontMed_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontLight_, h.text, base * 0.95f, base * 0.04f);
            drawText(fontLight_, q, accent, halo, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontMed_, h.text, base * 1.05f, base * 0.06f, -base * 0.08f);
                drawText(fontMed_, q, ink, halo, alpha);
            } else {
                place(fontLight_, h.text, base * 0.82f, base * 0.03f);
                drawText(fontLight_, q, h.accent ? accent : ink, halo, alpha);
            }
            continue;
        default:
            place(fontLight_, h.text, base * 0.8f, base * 0.02f);
            drawText(fontLight_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void PlanksUniverse::drawPlanks(GLuint prog, bool shadow) {
    if (inst_.empty()) return;
    glUseProgram(prog);
    glBindVertexArray(boxVao_);
    if (shadow) {
        // the previews are left out of the shadow maps
        if (previewStart_ > 0) glDrawArraysInstanced(GL_TRIANGLES, 0, 36, (GLsizei)previewStart_);
        if (previewEnd_ < inst_.size()) {
            // (draw the rest with a base instance offset: rebind the attributes at that offset)
            glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
            for (int i = 0; i < 5; i++)
                glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(Inst),
                                      (void*)(previewEnd_ * sizeof(Inst) + sizeof(float) * 4 * i));
            glDrawArraysInstanced(GL_TRIANGLES, 0, 36, (GLsizei)(inst_.size() - previewEnd_));
            for (int i = 0; i < 5; i++)
                glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(Inst), (void*)(sizeof(float) * 4 * i));
        }
        return;
    }
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, (GLsizei)inst_.size());
}

void PlanksUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    if (!bare_) syncCells(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) {
            // a new game: the toppled village fades away, a new one starts
            overT_ = -1.f;
            resetVillage();
            falls_.clear();
        } else overT_ += dt;
    }
    if (!topple_.empty() && overT_ < 0.f) {
        toppleFade_ = std::min(1.f, toppleFade_ + dt / 1.2f);
        if (toppleFade_ >= 1.f) topple_.clear();
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    time_ += f.paused ? 0.f : dt;
    worldTime_ += wdt;
    hasBeat_ = music.hasSong;
    beat_ = music.hasSong ? music.beat : worldTime_ * 2.0;
    beatLen_ = music.hasSong ? std::clamp(60.f / std::max(40.f, music.bpm), 0.25f, 1.5f) : 0.5f;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    swell_ = approach(swell_, 0.f, 0.35f, dt);
    glowAll_ = approach(glowAll_, 0.f, 1.5f, dt);
    if (!dropMode_) dropRelease_ = std::max(0.f, dropRelease_ - dt / 6.f);
    if (!f.paused) {
        for (int y = 0; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                settle_[y][x] = std::min(9.f, settle_[y][x] + dt);
                float& z = press_[y][x];
                float& v = pressV_[y][x];
                if (z == 0.f && v == 0.f) continue;
                const float a = -700.f * z - 38.f * v;
                v += a * dt;
                z += v * dt;
                if (std::fabs(z) < 0.0005f && std::fabs(v) < 0.01f) { z = 0.f; v = 0.f; }
            }
    }
    // the beat grid (every two beats): from mid on, a gentle ripple up the village
    if (!f.paused) {
        bool tick = false;
        if (music.hasSong) {
            const double hb = std::floor(music.beat / 2.0 + 1e-4);
            if (lastHalf_ < 0.0 || hb < lastHalf_ - 8.0) lastHalf_ = hb;
            if (hb > lastHalf_) { lastHalf_ = hb; tick = true; }
        } else {
            beatClock_ += wdt;
            if (beatClock_ > 1.f) { beatClock_ = 0.f; tick = true; }
        }
        if (tick && overT_ < 0.f && lv_ > 0.6f && rippleT_ > 0.9f) {
            rippleT_ = 0.f;
            rippleA_ = 0.025f + 0.03f * wp + 0.05f * dropRelease_;
        }
        rippleT_ += wdt;
    }
    (void)wm;
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);

    // ---- The game's lines: the test hooks, a new game.
    {
        const int lines = bare_ ? std::max(0, lines_) : linesHere(game);
        if (lines_ >= 0 && lines < lines_ && !game.over() && !bare_) {
            resetVillage();
            hookLines_ = 0;
        }
        lines_ = lines;
        // ZEN_PLANKS_TETRIS_AT=T: a Tetris' choreography at T seconds (four rows in the well's middle, for clips:
        // the autoplay never makes one)
        static const float tetrisAt = std::getenv("ZEN_PLANKS_TETRIS_AT") ? (float)std::atof(std::getenv("ZEN_PLANKS_TETRIS_AT")) : -1.f;
        if (tetrisAt >= 0.f && time_ >= tetrisAt && time_ - dt < tetrisAt && !bare_) {
            GameEvent ev{GameEvent::Clear};
            ev.count = 4;
            for (int y = 12; y < 16; y++)
                for (int x = 0; x < Game::W; x++) ev.cells.push_back({x, y, (x / 2 + y) % 7});
            onEvent(ev, game);
        }
        const int rate = hookRate(time_);
        while (hookLines_ < rate && overT_ < 0.f) {
            hookLines_++;
            GameEvent ev{GameEvent::Clear};
            ev.count = 1;
            for (int x = 0; x < Game::W; x++) ev.cells.push_back({x, Game::H - 1, rng_.irange(0, 6)});
            onEvent(ev, game);
        }
    }

    // ---- The train: it shuttles along its track at a peak, fast in the drop; it slows to turn at the ends.
    if (trainBuilt() && !toppled_) {
        const float target = (overT_ < 0.f ? (1.1f * wp + 2.8f * dropRelease_) : 0.f) * (1.f - 0.7f * slow_);
        trainV_ = approach(trainV_, target, 0.8f, dt);
        // (the right half only: from behind the well to the track's end, never under the left HUD column)
        const float lo = pb::trackLength() * 0.5f + 0.5f, hi = pb::trackLength() - 1.4f;
        const float ahead = trainDir_ > 0.f ? hi - trainS_ : trainS_ - lo;
        const float k = 0.12f + 0.88f * smoothstepf(0.f, 3.5f, ahead);
        trainS_ += trainDir_ * trainV_ * k * wdt;
        if (trainS_ > hi) { trainS_ = hi; trainDir_ = -1.f; }
        if (trainS_ < lo) { trainS_ = lo; trainDir_ = 1.f; }
    }
    updateFlights(f.paused ? 0.f : dt);
    if (!bare_) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- Geometry.
    inst_.clear();
    buildVillage();
    buildBoard(game);

    // ---- Light: a soft key from the variant's side (an evening light from the right in slow motion).
    const VariantDef& V = VARIANTS[variant_];
    const vec3 Lday = normalize(vec3(V.lx, V.ly, V.lz)), Leve = normalize(vec3(0.75f, 0.36f, 0.55f));
    const vec3 L = normalize(lerp(Lday, Leve, smoothstepf(0.f, 1.f, slow_)));
    const float keyK = V.key * (0.92f + 0.04f * wp + 0.06f * swell_);
    const vec3 keyCol = lerp(lin(V.keyCol), vec3(1.0f, 0.68f, 0.4f), slow_) * keyK * 1.05f;
    const vec3 tableNear = lin(V.tableNear), tableFar = lin(V.tableFar), wallTop = lin(V.wallTop);
    const vec3 poolCol = lin(V.pool) * (1.f + 0.6f * swell_ + 0.25f * wp);

    // the shadow maps: the village (far) and the well (near)
    auto lightMat = [&](const float xs[2], const float ys[2], const float zs[2], const vec3& center) {
        const mat4 lv = mat4::lookAt(center + L * 80.f, center, vec3(0, 1, 0));
        float mn[3] = {1e9f, 1e9f, 1e9f}, mx[3] = {-1e9f, -1e9f, -1e9f};
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++)
                for (int c = 0; c < 2; c++) {
                    const vec4 p = lv * vec4(xs[a], ys[b], zs[c], 1.f);
                    const float cc[3] = {p.x, p.y, p.z};
                    for (int i = 0; i < 3; i++) { mn[i] = std::min(mn[i], cc[i]); mx[i] = std::max(mx[i], cc[i]); }
                }
        return mat4::ortho(mn[0], mx[0], mn[1], mx[1], -mx[2] - 5.f, -mn[2] + 5.f) * lv;
    };
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    glBufferData(GL_ARRAY_BUFFER, inst_.size() * sizeof(Inst), inst_.data(), GL_STREAM_DRAW);
    {
        const float fx[2] = {-48.f, 48.f}, fy[2] = {TABLE - 0.5f, 26.f}, fz[2] = {-64.f, 3.f};
        const float nx[2] = {-7.f, 7.f}, ny[2] = {TABLE - 0.3f, 11.f}, nz[2] = {-1.4f, 1.f};
        lightVp_[0] = lightMat(fx, fy, fz, vec3(0.f, 0.f, -28.f));
        lightVp_[1] = lightMat(nx, ny, nz, vec3(0.f, 0.f, -0.3f));
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        for (int i = 0; i < 2; i++) {
            const int S = i == 0 ? 4096 : 2048;
            glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_[i]);
            glViewport(0, 0, S, S);
            glClearDepth(1.0);
            glClear(GL_DEPTH_BUFFER_BIT);
            glPolygonOffset(1.2f, 2.f);
            glUseProgram(progShadow_);
            setMat(progShadow_, "uVP", lightVp_[i]);
            setMat(progShadow_, "uView", view_);
            drawPlanks(progShadow_, true);
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    const float seedU = (float)(seed_ % 1000) * 0.137f;
    auto common = [&](GLuint p) {
        set1f(p, "uSeed", seedU);
        set1f(p, "uTable", TABLE);
        set1f(p, "uWall", WALL);
        set3f(p, "uTableNear", tableNear);
        set3f(p, "uTableFar", tableFar);
        set3f(p, "uWallTop", wallTop);
        set3f(p, "uPoolCol", poolCol);
        set3f(p, "uLightDir", L);
        set3f(p, "uLightCol", keyCol);
        set3f(p, "uEye", eye_);
    };
    auto shadowUniforms = [&](GLuint p) {
        setMat(p, "uLightVP", lightVp_[0]);
        setMat(p, "uLightVPn", lightVp_[1]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTex_[0]);
        set1i(p, "uShadow", 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, shadowTex_[1]);
        set1i(p, "uShadowN", 2);
        set1f(p, "uSoftFar", 4.5f / 4096.f);
        set1f(p, "uSoftNear", 5.f / 2048.f);
        glActiveTexture(GL_TEXTURE0);
    };

    // ---- The prepass and the ambient occlusion.
    glBindFramebuffer(GL_FRAMEBUFFER, preFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glUseProgram(progPreTable_);
    setMat(progPreTable_, "uInvVP", invVp_);
    setMat(progPreTable_, "uView", view_);
    setMat(progPreTable_, "uVP", vp_);
    set1f(progPreTable_, "uTable", TABLE);
    ugl::drawFullscreen();
    glUseProgram(progPre_);
    setMat(progPre_, "uVP", vp_);
    setMat(progPre_, "uView", view_);
    set1f(progPre_, "uClipZ", BOARD_Z);
    set1f(progPre_, "uClipY", -10.f);
    drawPlanks(progPre_, false);
    glDisable(GL_DEPTH_TEST);
    aoT_[0].bind();
    glUseProgram(progSsao_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prePos_);
    set1i(progSsao_, "uPos", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, preNrm_);
    set1i(progSsao_, "uNrm", 1);
    setMat(progSsao_, "uProj", proj_);
    set1f(progSsao_, "uRadius", 0.42f);
    set1f(progSsao_, "uStrength", 1.25f);
    ugl::drawFullscreen();
    for (int pass = 0; pass < 2; pass++) {
        aoT_[1 - pass].bind();
        glUseProgram(progBlur_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, aoT_[pass].tex);
        set1i(progBlur_, "uAO", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, prePos_);
        set1i(progBlur_, "uPos", 1);
        ugl::set2f(progBlur_, "uDir", pass == 0 ? 1.f / width_ : 0.f, pass == 0 ? 0.f : 1.f / height_);
        ugl::drawFullscreen();
    }
    // (the result is in aoT_[0])

    // ---- The studio, then the planks (multisampled).
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(progStudio_);
    common(progStudio_);
    shadowUniforms(progStudio_);
    setMat(progStudio_, "uInvVP", invVp_);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, aoT_[0].tex);
    set1i(progStudio_, "uAO", 3);
    glActiveTexture(GL_TEXTURE0);
    set1f(progStudio_, "uShadowK", ugl::luma(tableNear) > 0.25f ? 0.62f : 0.5f);
    set1f(progStudio_, "uGrain", 0.014f);
    ugl::drawFullscreen();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glUseProgram(progPlank_);
    common(progPlank_);
    shadowUniforms(progPlank_);
    setMat(progPlank_, "uVP", vp_);
    setMat(progPlank_, "uView", view_);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, aoT_[0].tex);
    set1i(progPlank_, "uAO", 3);
    glActiveTexture(GL_TEXTURE0);
    const float ambK = V.amb * (1.f - 0.25f * slow_);
    set3f(progPlank_, "uAmbTop", mixDisplay(wallTop, vec3(0.8f, 0.85f, 0.95f), 0.3f) * (0.5f * ambK));
    set3f(progPlank_, "uAmbBot", tableNear * (0.3f * ambK));
    set3f(progPlank_, "uFillDir", vec3(0.65f, 0.25f, 0.75f));
    set3f(progPlank_, "uFillCol", vec3(0.8f, 0.86f, 1.0f) * (0.12f * ambK));
    set3f(progPlank_, "uEarly", lin(V.early));
    set3f(progPlank_, "uLate", lin(V.late));
    set1f(progPlank_, "uShadowK", 0.85f);
    set1f(progPlank_, "uSheen", V.sheen);
    set1f(progPlank_, "uClipZ", BOARD_Z);
    set1f(progPlank_, "uClipY", -10.f);
    drawPlanks(progPlank_, false);

    if (!flat_.empty()) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
        glDepthMask(GL_FALSE);
        glUseProgram(progFlat_);
        setMat(progFlat_, "uVP", vp_);
        glBindVertexArray(flatVao_);
        glBindBuffer(GL_ARRAY_BUFFER, flatVbo_);
        glBufferData(GL_ARRAY_BUFFER, flat_.size() * sizeof(float), flat_.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(flat_.size() / 7));
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glDisable(GL_DEPTH_TEST);

    // ---- Resolve, then the frame.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    set1f(progFinal_, "uExposure", 1.f);
    set1f(progFinal_, "uSlow", slow_ * 0.6f);
    set3f(progFinal_, "uSlowTint", vec3(1.0f, 0.86f, 0.72f));
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

const UniverseEntry& universe_planks() {
    static const UniverseEntry e{
        "PLANKS", "wooden plank blocks in a white studio: every cleared row pushes its planks out to build a plank village",
        [] { return std::unique_ptr<Universe>(new PlanksUniverse()); },
        // Calm, warm, acoustic songs suit the wooden studio best; it fits most songs.
        [](const SongLook& l) { return 0.8f + 0.2f * (1.f - l.airWeight) + 0.15f * l.brightness; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
