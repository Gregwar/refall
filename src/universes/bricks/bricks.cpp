// BRICKS: glossy toy bricks in a photo studio; every cleared row is ejected as bricks that build models turning on
// turntables beside the well.
//
// One world, in real 3D: a seamless studio sweep (a table curving up into the backdrop) under one soft key light,
// seen through the layout's camera raised a little and looking straight ahead with a lens shift, so the board's plane
// keeps its pixels while the table recedes in perspective. The well is a plastic board standing on a floor plate; the
// pieces are interlocking plastic bricks clipped onto it: one brick per piece (its cells merged, molded bevels on its
// free edges, a stud on each top face left open), glossy ABS (a soft highlight, the softbox in the reflection, a
// little warmth on the shadow side), real shadows from a shadow map (the falling piece is held a little off the board:
// only its shadow says so). On each side of the well, in the margins, a turntable carries a model being built: a
// cottage, a lighthouse, an apple tree, a windmill, a castle tower, a rocket, a pagoda, a duck, a sailboat. They are
// built from the play: a cleared row pops off the board, each run of one piece in it a 1 x N brick sliding out of the
// well (the left half to the left, the right half to the right), and flies in an arc to the model, where it becomes
// its next brick (one of the bricks the model can take now, of the brick's color if one is waiting for it, else
// repainted on the way) and snaps into place. A finished model turns once, glowing softly, and leaves for the gallery
// at the back of the table, where the play's models stay, receding into the studio's haze.
//
// The song: outside the drop a model never comes apart. A calm part is a slow turntable under a soft light; mid parts
// lift each model gently, whole, on the beat grid (every two beats); a peak lights the studio a little more, lifts them
// a little higher and its kicks give them a soft glow. The drop spins the turntables up and the models come apart and
// back together every two bars, in step with the beat grid (a spiral staircase, an exploded view, a twisted column),
// and come home quickly when it ends. A lock is the piece clicking onto the board; a Tetris throws its bricks high and
// the models glow, lift and turn once. Slow motion is an
// evening light raking in from the right; game over pops the stack's bricks off the board, from the top down.
#include "universes/bricks/bricks.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/bricks/models.hpp"
#include "universes/bricks/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Outfit (Medium and Light), SIL Open Font License, see OFL-Outfit.txt.
const unsigned char FONT_MED[] = {
#include "universes/bricks/Outfit-Medium.ttf.inc"
};
const unsigned char FONT_LIGHT[] = {
#include "universes/bricks/Outfit-Light.ttf.inc"
};

namespace shaders = bricks_shaders;
namespace bm = bricks_models;
using ugl::set1f;
using ugl::set1i;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
vec3 mixDisplay(const vec3& a, const vec3& b, float t) { return ugl::toLinear(lerp(ugl::toSrgb(a), ugl::toSrgb(b), t)); }

constexpr float EYE_Y = 5.0f;     // the camera's height (board units)
constexpr float TABLE = -10.4f;   // the table's plane (the floor plate's foot)
constexpr float WALL = -240.f;    // the backdrop
constexpr float HOLD_Z = 0.32f;   // the falling piece, held off the board
constexpr float P = 0.75f;        // a model's stud pitch
constexpr float PLATE = 0.4f * P; // a plate's height (a brick is three)
constexpr float BRICK_H = 1.2f * P;
constexpr float TT_R = 4.45f, TT_H = 0.42f; // the turntables
constexpr float TABLE_Z = -16.f;  // the turntables' depth
constexpr float SNAP_H = 0.55f;   // a brick arrives that far above its place, then snaps down
constexpr int GALLERY = 4;        // finished models kept per side

// The studios: the table (near, far), the backdrop's top, the light pool, the board, the turntables, the seven bricks
// (I O T S Z J L), the neutrals (white, dark, tan, grey), the accent, the key light's strength, and the drop's glow.
struct VariantDef {
    const char* name;
    unsigned tableNear, tableFar, wallTop, pool, board, turntable;
    unsigned piece[7];
    unsigned neutral[4];
    unsigned accent;
    float key, amb;
    unsigned dropGlow;
};
const VariantDef VARIANTS[] = {
    // a white studio, the classic primaries
    {"CLASSIC", 0xefebe4, 0xe0dbd2, 0xd2ccc3, 0x18140f, 0xc9c4bb, 0x8e8a84,
     {0x2f9fdc, 0xf6c21c, 0x8a4cc2, 0x2ea64e, 0xde3a2e, 0x2453c4, 0xf27a1c},
     {0xf3f1ec, 0x2b2b30, 0xc99f62, 0x9aa0a7}, 0xde3a2e, 1.0f, 1.0f, 0xffb070},
    // a blush studio, bright fruit-sorbet bricks (saturated, not pastel: blue raspberry, lemon, grape, lime, raspberry,
    // blueberry, tangerine) on a deep plum board, a lower ambient so the pale studio does not wash them out
    {"SORBET", 0xf4e5df, 0xe9d5cf, 0xdcc3be, 0x1a120f, 0x4a3046, 0x8a7282,
     {0x34c6e6, 0xffd030, 0xb062f0, 0x74d84c, 0xff4f80, 0x5878f4, 0xff8a3c},
     {0xfaf4ee, 0x3e2a3c, 0xd9a06a, 0xb5a2ae}, 0xff4f80, 1.0f, 0.85f, 0xff9f8a},
    // a night studio, bright bricks under one lamp
    {"NIGHT", 0x252b3b, 0x141824, 0x0b0e16, 0x1b1d26, 0x2a3142, 0x3a4152,
     {0x3fc4ff, 0xffd23f, 0xb46cff, 0x46dc88, 0xff4d5c, 0x4f7dff, 0xff8a38},
     {0xe6eaf0, 0x1b1e26, 0xc89a62, 0x7d8696}, 0xffd23f, 1.05f, 0.7f, 0xff9a50},
    // a sand studio, earth-toned bricks on a walnut board
    {"EARTH", 0xe8d9c3, 0xd8c5aa, 0xc8b394, 0x18120a, 0x5c4a3c, 0x7a6450,
     {0x4f9aa3, 0xe2a336, 0x8e5a80, 0x7c9a44, 0xbf4a2e, 0x3f5f8e, 0xdc7636},
     {0xf1e7d3, 0x3a2f28, 0xb98a55, 0x958a7e}, 0xbf4a2e, 1.0f, 0.95f, 0xffa860},
    // a slate studio, vivid bricks
    {"SLATE", 0x4c5460, 0x3a414c, 0x2c323b, 0x181a1e, 0x2b3038, 0x5a616c,
     {0x4cc3e6, 0xffcf3a, 0xa866e8, 0x4cc96e, 0xf0503c, 0x3d6fe0, 0xff8a2a},
     {0xf2f3f5, 0x22252a, 0xd4a66a, 0xa9b0ba}, 0xffcf3a, 1.0f, 0.8f, 0xffa050},
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

// ---- quaternions
struct Q { float x = 0, y = 0, z = 0, w = 1; };
Q qAxis(const vec3& axis, float ang) {
    const vec3 a = normalize(axis);
    const float s = std::sin(ang * 0.5f);
    return {a.x * s, a.y * s, a.z * s, std::cos(ang * 0.5f)};
}
Q qYaw(float ang) { return {0.f, std::sin(ang * 0.5f), 0.f, std::cos(ang * 0.5f)}; }
Q qMul(const Q& a, const Q& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
vec3 qRot(const Q& q, const vec3& v) {
    const vec3 u(q.x, q.y, q.z);
    return v + cross(u, cross(u, v) + v * q.w) * 2.f;
}
Q qSlerp(Q a, const Q& b, float t) {
    float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (d < 0) { a = {-a.x, -a.y, -a.z, -a.w}; d = -d; }
    float k0, k1;
    if (d > 0.9995f) { k0 = 1 - t; k1 = t; }
    else {
        const float th = std::acos(d), s = std::sin(th);
        k0 = std::sin((1 - t) * th) / s;
        k1 = std::sin(t * th) / s;
    }
    Q r{a.x * k0 + b.x * k1, a.y * k0 + b.y * k1, a.z * k0 + b.z * k1, a.w * k0 + b.w * k1};
    const float n = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
    return {r.x / n, r.y / n, r.z / n, r.w / n};
}
float easeOutBack(float t) {
    const float c1 = 1.9f, c3 = c1 + 1.f;
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

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool BricksUniverse::init(int width, int height) {
    using shaders::withCommon;
    progStudio_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::STUDIO_FS).c_str(), "bricks studio");
    progBrick_ = ugl::program(shaders::BRICK_VS, withCommon(shaders::BRICK_FS).c_str(), "bricks solids");
    progShadow_ = ugl::program(shaders::BRICK_VS, shaders::SHADOW_FS, "bricks shadow");
    progFlat_ = ugl::program(shaders::FLAT_VS, shaders::FLAT_FS, "bricks flat");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "bricks final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "bricks shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "bricks text");

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
    // a stud: a cylinder of radius 1 and height 1 (its foot at y = 0), a small chamfer around its top
    std::vector<float> stud;
    {
        const int S = 28;
        const float ch = 0.2f, cr = 0.07f;
        auto V = [&](float r, float y, float a, const vec3& n) {
            const float v[6] = {r * std::cos(a), y, -r * std::sin(a), n.x, n.y, n.z};
            stud.insert(stud.end(), v, v + 6);
        };
        for (int i = 0; i < S; i++) {
            const float a0 = TAU * i / S, a1 = TAU * (i + 1) / S;
            const vec3 n0(std::cos(a0), 0, -std::sin(a0)), n1(std::cos(a1), 0, -std::sin(a1));
            // side
            V(1, 0, a0, n0); V(1, 0, a1, n1); V(1, 1 - ch, a1, n1);
            V(1, 0, a0, n0); V(1, 1 - ch, a1, n1); V(1, 1 - ch, a0, n0);
            // chamfer
            const vec3 c0 = normalize(n0 + vec3(0, 1.2f, 0)), c1 = normalize(n1 + vec3(0, 1.2f, 0));
            V(1, 1 - ch, a0, c0); V(1, 1 - ch, a1, c1); V(1 - cr, 1, a1, c1);
            V(1, 1 - ch, a0, c0); V(1 - cr, 1, a1, c1); V(1 - cr, 1, a0, c0);
            // cap
            V(1 - cr, 1, a0, vec3(0, 1, 0)); V(1 - cr, 1, a1, vec3(0, 1, 0)); V(0, 1, 0, vec3(0, 1, 0));
        }
        studVerts_ = (int)(stud.size() / 6);
    }
    auto makeMesh = [&](GLuint& vao, GLuint& vbo, GLuint& ivbo, const std::vector<float>& data) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ivbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
        glBindBuffer(GL_ARRAY_BUFFER, ivbo);
        for (int i = 0; i < 5; i++) {
            glEnableVertexAttribArray(2 + i);
            glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(Inst), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(2 + i, 1);
        }
    };
    makeMesh(boxVao_, boxVbo_, instVbo_, box);
    makeMesh(studVao_, studVbo_, studInstVbo_, stud);

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

    // the shadow map (one soft key light)
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 4096, 4096, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
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
    // the stack's cells, for the board's ambient occlusion
    glGenTextures(1, &occTex_);
    glBindTexture(GL_TEXTURE_2D, occTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, Game::W, Game::H, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    buildFont(fontMed_, FONT_MED);
    buildFont(fontLight_, FONT_LIGHT);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void BricksUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] bricks font init failed\n");
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
void BricksUniverse::resize(int width, int height) {
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
    vp_ = shift * mat4::perspective(2.f * half, aspect, 0.5f, 1200.f) * view;
    // the inverse (for the studio's rays): the shift and the projection inverted analytically, then the view
    {
        mat4 pinv = mat4::identity();
        const float zn = 0.5f, zf = 1200.f;
        const float a = (zf + zn) / (zn - zf), b = 2 * zf * zn / (zn - zf);
        pinv.at(0, 0) = aspect / focal;
        pinv.at(1, 1) = 1.f / focal;
        pinv.at(2, 2) = 0.f;
        pinv.at(2, 3) = -1.f;
        pinv.at(3, 2) = 1.f / b;
        pinv.at(3, 3) = a / b;
        mat4 sinv = mat4::identity();
        sinv.at(1, 3) = -shift.at(1, 3);
        mat4 vinv = mat4::identity();
        vinv.at(0, 3) = eye_.x; vinv.at(1, 3) = eye_.y; vinv.at(2, 3) = eye_.z;
        invVp_ = vinv * pinv * sinv;
    }
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false);
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

BricksUniverse::~BricksUniverse() {
    sceneT_.destroy();
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    for (GLuint t : {shadowTex_, occTex_})
        if (t) glDeleteTextures(1, &t);
    for (GLuint p : {progStudio_, progBrick_, progShadow_, progFlat_, progFinal_, progShape_, progText_}) glDeleteProgram(p);
    for (GLuint v : {boxVao_, studVao_, flatVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {boxVbo_, studVbo_, instVbo_, studInstVbo_, flatVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontMed_, &fontLight_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

vec3 BricksUniverse::pieceColor(int t) const { return lin(VARIANTS[variant_].piece[std::clamp(t, 0, 6)]); }

vec3 BricksUniverse::backdrop() const {
    const VariantDef& V = VARIANTS[variant_];
    return mixDisplay(lin(V.tableFar), lin(V.wallTop), 0.3f);
}

void BricksUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = h < 0.5f ? 2 : 4;     // night, slate
        else if (t.hasSong && t.brightness > 0.6f) v = h < 0.5f ? 0 : 1;            // classic, sorbet
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0xB71Cull);
    if (seconds <= 0.1f || models_.empty()) {
        // a new show: two empty baseplates, the models in a shuffled order (each side its own)
        models_.clear();
        flights_.clear();
        debris_.clear();
        const int n = (int)bm::all().size();
        int perm[16];
        for (int i = 0; i < n; i++) perm[i] = i;
        for (int i = n - 1; i > 0; i--) std::swap(perm[i], perm[rng_.irange(0, i)]);
        for (int s = 0; s < 2; s++) {
            for (int i = 0; i < 16; i++) order_[s][i] = perm[(i * 2 + s) % n];
            orderPos_[s] = 0;
            queued_[s] = 0;
            lastArrive_[s] = time_;
            Model& m = newModel(s, time_ - 1.f);
            m.appear = 1.f;
            cur_[s] = m.id;
        }
    }
}

void BricksUniverse::setLevel(int level, float energy, float seconds, float impact) {
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
    (void)impact;
}

// The drop: the turntables spin up and the models come apart and back together on the beat grid.
void BricksUniverse::drop(float strength) {
    if (!dropMode_ || dropRelease_ <= 0.01f) {
        dropBeat0_ = hasBeat_ ? std::floor(beat_ / 2.0 + 0.5) * 2.0 : worldTime_ * 2.0;
        dropCycle_ = -1;
    }
    dropMode_ = true;
    dropRelease_ = 1.f;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

// A new section: the models on the turntables lift gently, whole (outside the drop a model never comes apart).
void BricksUniverse::refresh(float strength) {
    if (strength > 0.3f && lv_ > 0.5f) liftModels(1.f);
}

void BricksUniverse::levelUp() { liftModels(0.8f); }

// Both models on their turntables rise a little and settle (the whole model, its bricks never move apart).
void BricksUniverse::liftModels(float amount) {
    for (int s = 0; s < 2; s++) {
        if (liftT_[s] < 0.4f) continue;
        liftT_[s] = 0.f;
        liftA_[s] = amount;
    }
}

vec2 BricksUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints BricksUniverse::hints() const {
    TransitionHints h;
    const VariantDef& V = VARIANTS[variant_];
    h.background = backdrop();
    h.light = ugl::luma(h.background) > 0.25f;
    h.ink = h.light ? lin(0x2a2a30) : lin(0xf2efe8);
    h.accent = lin(V.accent);
    h.styles = TS_REBUILD | TS_TILES | TS_LUMA;
    return h;
}

// =====================================================================================================
// The models
// =====================================================================================================

BricksUniverse::Model* BricksUniverse::model(int id) {
    if (!id) return nullptr;
    for (Model& m : models_)
        if (m.id == id) return &m;
    return nullptr;
}

// A turntable's center on the table: in the margin, its model seen beside the HUD column.
vec3 BricksUniverse::tableCenter(int side) const {
    const layout::Camera ref = layout::camera(width_, height_);
    const float s = ref.dist / (ref.dist - TABLE_Z);
    const float aspect = (float)width_ / height_;
    const float halfW = ref.dist * std::tan(ref.fovDeg * 0.5f * PI / 180.f) * aspect;
    // the margin's middle (between the HUD column and the screen's edge), at least clear of the column
    const float sx = std::max(15.4f, 0.5f * (11.6f + halfW) - 0.9f);
    return vec3((side ? 1.f : -1.f) * sx / s, TABLE, TABLE_Z);
}

BricksUniverse::Model& BricksUniverse::newModel(int side, float readyAt) {
    Model m;
    m.id = nextModelId_++;
    m.side = side;
    m.def = order_[side][orderPos_[side] % 16];
    orderPos_[side]++;
    const bm::ModelDef& d = bm::all()[m.def];
    const size_t n = d.bricks.size();
    m.state.assign(n, 0);
    m.snap.assign(n, 1.f);
    m.readyAt = readyAt;
    m.appear = 0.f;
    m.pos = tableCenter(side) + vec3(0.f, TT_H, 0.f);
    m.yaw = tableYaw_[side];
    models_.push_back(m);
    return models_.back();
}

// The model a brick ejected now from this side goes to: the one on the turntable while it still needs bricks, else the
// next one (it comes onto the turntable once the finished one has left).
int BricksUniverse::targetModel(int side, float when) {
    Model* c = model(cur_[side]);
    if (c && c->reserved < (int)c->state.size()) return c->id;
    Model* q = model(queued_[side]);
    if (!q) {
        // the finished model leaves 2.4 s after its last brick, the next baseplate drops in after it
        const float ready = std::max(when, lastArrive_[side]) + 0.3f + 1.9f + 0.7f;
        Model& m = newModel(side, ready);
        queued_[side] = m.id;
        return m.id;
    }
    return q->id;
}

// The brick a run of `len` studs of piece `colorType` becomes: among the bricks the model can take now (bottom-up, a
// course at a time, nothing missing under it), one of its color, the closest in length; else the next one (repainted).
int BricksUniverse::pickBrick(Model& m, int colorType, int len) {
    const bm::ModelDef& d = bm::all()[m.def];
    const int n = (int)d.bricks.size();
    int minLayer = 999;
    for (int i = 0; i < n; i++)
        if (m.state[i] == 0) minLayer = std::min(minLayer, d.bricks[i].layer);
    if (minLayer == 999) return -1;
    auto overlap = [&](const bm::BrickDef& a, const bm::BrickDef& b) {
        return a.x0 < b.x0 + b.fx && b.x0 < a.x0 + a.fx && a.z0 < b.z0 + b.fz && b.z0 < a.z0 + a.fz;
    };
    int best = -1, rank = 0;
    float bestScore = -1e9f;
    for (int i = 0; i < n && rank < 10; i++) {
        if (m.state[i] != 0) continue;
        const bm::BrickDef& b = d.bricks[i];
        if (b.layer > minLayer + 1) break;
        bool supported = b.layer == 0, blocked = false;
        for (int j = 0; j < n && !blocked; j++) {
            const bm::BrickDef& o = d.bricks[j];
            if (o.layer >= b.layer || !overlap(o, b)) continue;
            if (m.state[j] == 0) blocked = true;
            if (o.layer == b.layer - 1) supported = true;
        }
        if (blocked) continue;
        // a brick hanging over nothing waits for its own course
        if (!supported && b.layer != minLayer) continue;
        if (b.group && b.layer != minLayer && b.layer > d.hubY) continue;
        float score = -2.f * rank;
        if (bm::ROLE_PIECE[b.role] == colorType) score += 12.f;
        score -= 1.5f * std::abs(b.len * b.wid - len);
        if (b.layer == minLayer) score += 3.f;
        if (score > bestScore) { bestScore = score; best = i; }
        rank++;
    }
    if (best < 0) {
        for (int i = 0; i < n; i++)
            if (m.state[i] == 0) return i;
    }
    return best;
}

vec3 BricksUniverse::brickSizeWorld(int def, int i) const {
    const bm::BrickDef& b = bm::all()[def].bricks[i];
    return vec3(b.len * P - 0.025f, BRICK_H - 0.012f, b.wid * P - 0.025f);
}

vec3 BricksUniverse::brickColor(int def, int i) const {
    const VariantDef& V = VARIANTS[variant_];
    const int r = bm::all()[def].bricks[i].role;
    if (r < 7) return lin(V.piece[bm::ROLE_PIECE[r]]);
    return lin(V.neutral[r - 7]);
}

// A brick of a model in the world (its center) and its turn; withMotion adds its snap and the drop's choreography
// (the only time a model comes apart).
vec3 BricksUniverse::brickWorld(const Model& m, int i, Quat* qo, bool withMotion) const {
    const bm::ModelDef& d = bm::all()[m.def];
    const bm::BrickDef& b = d.bricks[i];
    vec3 c((b.x0 + b.fx * 0.5f - d.W * 0.5f) * P, PLATE + (b.layer + 0.5f) * BRICK_H, (b.z0 + b.fz * 0.5f - d.D * 0.5f) * P);
    Q q = qYaw(b.yaw);
    if (b.group == 1) {
        // the windmill's sails turn about their hub
        const vec3 hub((d.hubX - d.W * 0.5f) * P, PLATE + d.hubY * BRICK_H, (d.hubZ - d.D * 0.5f) * P);
        const Q r = qAxis(vec3(0, 0, 1), m.sail);
        c = hub + qRot(r, c - hub);
        q = qMul(r, q);
    }
    if (withMotion) {
        if (m.snap[i] < 1.f) c.y += SNAP_H * (1.f - easeOutBack(m.snap[i]));
        if (m.onTable && dropS_ > 0.f && i < (int)m.rank.size()) {
            // the drop's choreography (top bricks leave first and come back last, so it rebuilds bottom-up); the
            // bricks keep their poise: no tumbling, each turning smoothly to its place in the figure
            const int n = std::max(1, m.reserved);
            const int ri = m.rank[i];
            const float k = n > 1 ? (float)ri / (n - 1) : 0.f;
            const float stag = 0.45f;
            const float s = smoother((dropS_ * (1.f + stag) - stag * (1.f - k)));
            if (s > 0.f) {
                vec3 tgt;
                float yaw = 0.f, tilt = 0.f;
                const float bob = 0.12f * std::sin(worldTime_ * 2.1f + ri * 0.7f);
                if (dropForm_ == 0) {
                    // a helix opening upward around the turntable's axis, each brick along it
                    // (a spiral staircase: each brick a step after the one before)
                    const float th = ri * 0.62f;
                    const float r = 2.7f;
                    tgt = vec3(r * std::cos(th), 1.2f + ri * std::min(0.3f, 10.f / n) + bob * 0.3f, r * std::sin(th));
                    yaw = -th + PI * 0.5f;
                } else if (dropForm_ == 1) {
                    // an exploded view: the model opens up around its heart, its courses apart, as in a building
                    // guide, and breathes there
                    const float lift = PLATE + 0.5f * BRICK_H;
                    tgt = vec3(c.x * 1.6f, lift + (c.y - lift) * 1.55f + 0.5f + bob * 0.5f, c.z * 1.6f);
                    (void)tilt;
                } else {
                    // a twisted column: the bricks stacked in rings, each ring turned a little more
                    const int per = std::max(3, (n + 15) / 16);
                    const int ring = ri / per, slot = ri % per;
                    const float th = TAU * slot / per + ring * 0.42f;
                    const float r = 0.75f + 0.12f * per;
                    tgt = vec3(r * std::cos(th), 1.0f + ring * 0.78f + bob * 0.3f, r * std::sin(th));
                    yaw = -th + PI * 0.5f;
                }
                const vec3 base = c;
                c = lerp(base, tgt, s) + vec3(0.f, std::sin(PI * s) * 0.8f, 0.f);
                Q qt = qYaw(yaw);
                if (tilt != 0.f) {
                    const vec3 o = normalize(vec3(tgt.x, 0.f, tgt.z) + vec3(1e-3f, 0.f, 0.f));
                    qt = qMul(qAxis(cross(vec3(0, 1, 0), o), tilt), q);
                }
                q = qSlerp(q, qt, s);
            }
        }
    }
    if (qo) {
        const Q w = qMul(qYaw(m.yaw), q);
        qo->x = w.x; qo->y = w.y; qo->z = w.z; qo->w = w.w;
    }
    return m.pos + qRot(qYaw(m.yaw), c);
}

// The gallery: the finished models of a side, receding along the table into the haze.
void BricksUniverse::layoutGallery(int side) {
    const layout::Camera ref = layout::camera(width_, height_);
    const float aspect = (float)width_ / height_;
    const float halfW = ref.dist * std::tan(ref.fovDeg * 0.5f * PI / 180.f) * aspect;
    std::vector<Model*> g;
    for (Model& m : models_)
        if (!m.onTable && !m.dying && m.side == side) g.push_back(&m);
    std::sort(g.begin(), g.end(), [](const Model* a, const Model* b) { return a->slot < b->slot; });
    for (size_t k = 0; k < g.size(); k++) {
        Model& m = *g[k];
        const int slot = (int)k;
        const vec3 cur = m.pos;
        const float z = -46.f - 30.f * slot;
        const float s = ref.dist / (ref.dist - z);
        const float sx = (slot % 2 == 0) ? std::min(halfW - 2.6f, 19.0f) : 13.2f;
        vec3 to((side ? 1.f : -1.f) * sx / s, TABLE, z);
        float fogTo = 0.1f + 0.13f * slot;
        if (slot >= GALLERY) {
            m.dying = true;
            to = to + vec3(0.f, 0.f, -60.f);
            fogTo = 1.f;
        }
        if (m.slot == slot && m.moveT >= 1.f && !m.dying) continue;
        m.slot = slot;
        m.from = cur;
        m.to = to;
        m.moveT = 0.f;
        m.yawFrom = m.yaw;
        m.yawTo = std::round(m.yaw / TAU) * TAU + (side ? -0.55f : 0.55f);
        m.fogTo = fogTo;
    }
}

void BricksUniverse::startGallery(Model& m) {
    m.onTable = false;
    m.slot = -1;
    m.glow = 0.f;
    layoutGallery(m.side);
}

void BricksUniverse::updateModels(float dt, float wdt) {
    // the turntables: slow, livelier at a peak, fast in the drop (eased), nearly still in slow motion
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    for (int s = 0; s < 2; s++) {
        const float target = (0.11f + 0.05f * wm + 0.06f * wp + 0.75f * dropRelease_) * (1.f - 0.8f * slow_);
        tableSpin_[s] = approach(tableSpin_[s], target, 0.9f, dt);
        const float dir = s ? -1.f : 1.f;
        tableYaw_[s] += dir * tableSpin_[s] * (overT_ >= 0.f ? 0.4f : 1.f) * wdt;
    }

    for (Model& m : models_) {
        const size_t n = m.state.size();
        m.rank.assign(n, 0);
        int r = 0;
        for (size_t i = 0; i < n; i++)
            if (m.state[i] != 0) m.rank[i] = r++;
        for (size_t i = 0; i < n; i++) {
            if (m.snap[i] < 1.f) m.snap[i] = std::min(1.f, m.snap[i] + dt / 0.28f);
        }
        m.glow = approach(m.glow, 0.f, 1.2f, dt);
        // a windmill's sails start turning once it is finished (faster with the music, fastest in the drop)
        if (m.placed >= (int)n) {
            const float target = (0.35f + 0.5f * wm + 0.9f * dropRelease_) * (m.onTable ? 1.f : 0.5f);
            m.sailV = approach(m.sailV, target, 0.6f, dt);
            m.sail += m.sailV * wdt;
        }
        if (m.onTable) {
            if (time_ >= m.readyAt) m.appear = std::min(1.f, m.appear + dt / 0.6f);
            const float extra = m.doneT >= 0.f ? TAU * smoother(m.doneT / 1.8f) : 0.f;
            m.yaw = tableYaw_[m.side] + (m.side ? -extra : extra);
            // the beat grid's gentle lift of the whole model (a smooth bump, zero speed at both ends)
            const float lu = saturate(liftT_[m.side] / 0.7f);
            const float sb = std::sin(PI * lu);
            m.pos = tableCenter(m.side) + vec3(0.f, TT_H + 0.22f * liftA_[m.side] * sb * sb, 0.f);
            if (m.placed >= (int)n && m.doneT < 0.f) {
                // finished: a soft glow and one turn, then it leaves for the gallery
                m.doneT = 0.f;
            }
            if (m.doneT >= 0.f) {
                m.doneT += dt;
                m.glow = std::max(m.glow, 0.12f * std::sin(PI * saturate(m.doneT / 1.8f)));
                if (m.doneT >= 1.9f) {
                    const int side = m.side, id = m.id;
                    // the next model takes the turntable
                    Model* q = model(queued_[side]);
                    int next;
                    if (q) {
                        next = q->id;
                        queued_[side] = 0;
                        q->readyAt = std::min(q->readyAt, time_);
                    } else {
                        Model& nm = newModel(side, time_);
                        next = nm.id;
                    }
                    if (Model* old = model(id)) startGallery(*old);
                    cur_[side] = next;
                    break; // models_ may have grown: start over next frame
                }
            }
        } else {
            if (m.moveT < 1.f) {
                m.moveT = std::min(1.f, m.moveT + dt / (m.dying ? 4.f : 3.2f));
                const float e = smoother(m.moveT);
                m.pos = lerp(m.from, m.to, e) + vec3(0.f, std::sin(PI * e) * (m.dying ? 0.f : 2.2f), 0.f);
                m.yaw = lerpf(m.yawFrom, m.yawTo, e);
                m.fog = lerpf(m.fog, m.fogTo, e);
            }
        }
    }
    models_.erase(std::remove_if(models_.begin(), models_.end(),
                                 [&](const Model& m) { return m.dying && m.moveT >= 1.f; }),
                  models_.end());
}

// =====================================================================================================
// Flights
// =====================================================================================================

void BricksUniverse::updateFlights(float dt, float wdt) {
    (void)wdt;
    for (Flight& f : flights_) {
        f.t += dt;
        if (f.phase == 0 && f.t >= f.slideDur) {
            f.phase = 1;
            f.t -= f.slideDur;
            f.arcFrom = f.slideTo;
        }
        if (f.phase == 1 && f.t >= f.arcDur) {
            f.phase = 3;
            if (Model* m = model(f.model)) {
                m->state[f.brick] = 2;
                m->snap[f.brick] = 0.f;
                m->placed++;
            }
        }
    }
    flights_.erase(std::remove_if(flights_.begin(), flights_.end(), [](const Flight& f) { return f.phase == 3; }),
                   flights_.end());
    for (Debris& d : debris_) {
        if (d.delay > 0.f) { d.delay -= dt; continue; }
        d.t += dt;
        d.v.y -= 22.f * dt;
        d.p = d.p + d.v * dt;
        d.ang += d.w * dt;
    }
    debris_.erase(std::remove_if(debris_.begin(), debris_.end(), [](const Debris& d) { return d.p.y < -22.f; }),
                  debris_.end());
}

// =====================================================================================================
// Game events
// =====================================================================================================

void BricksUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) { ids_[y][x] = 0; press_[y][x] = pressV_[y][x] = 0.f; }
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
                const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& dd : d) {
                    const int nx = cx + dd[0], ny = cy + dd[1];
                    if (nx < 0 || ny < 0 || nx >= Game::W || ny >= Game::H || ids_[ny][nx]) continue;
                    if (g.cell(nx, ny).type != g.cell(x, y).type || sp >= 8 || count + sp >= 4) continue;
                    ids_[ny][nx] = id;
                    stack[sp][0] = nx; stack[sp][1] = ny; sp++;
                }
            }
        }
}

void BricksUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        // the piece clicks onto the board's studs
        const int id = nextId_++;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            press_[c.y][c.x] = HOLD_Z;
            pressV_[c.y][c.x] = -1.5f;
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        auto typeAt = [&](int x, int y) {
            for (auto& c : ev.cells)
                if (c.x == x && c.y == y) return std::clamp(c.type, 0, 6);
            return 0;
        };
        const bool tetris = ev.count >= 4;
        int rowK = -1;
        for (int y = Game::H - 1; y >= Game::HIDDEN; y--) {
            if (!rows[y]) continue;
            rowK++;
            // the row's runs of one piece: 1 x N bricks
            struct Run { int x0, x1, type; };
            std::vector<Run> runs;
            int x = 0;
            while (x < Game::W) {
                const int id = ids_[y][x], type = typeAt(x, y);
                int x1 = x + 1;
                while (x1 < Game::W && x1 - x < 4 &&
                       (id ? ids_[y][x1] == id : (!ids_[y][x1] && typeAt(x1, y) == type)))
                    x1++;
                runs.push_back({x, x1 - 1, type});
                x = x1;
            }
            // each half slides out of the well as one block (its innermost edge passes the board's side)
            float edgeL = -99.f, edgeR = 99.f;
            for (const Run& r : runs) {
                const float c = layout::cellPos(0.5f * (r.x0 + r.x1), (float)y).x;
                const float e0 = layout::cellPos((float)r.x0, (float)y).x - 0.5f, e1 = layout::cellPos((float)r.x1, (float)y).x + 0.5f;
                if (c < 0.f) edgeL = std::max(edgeL, e1);
                else edgeR = std::min(edgeR, e0);
            }
            // outermost first on each side (the order they land in)
            std::vector<int> order;
            for (int i = 0; i < (int)runs.size(); i++) order.push_back(i);
            std::sort(order.begin(), order.end(), [&](int a, int b) {
                const float ca = std::fabs(layout::cellPos(0.5f * (runs[a].x0 + runs[a].x1), 0.f).x);
                const float cb = std::fabs(layout::cellPos(0.5f * (runs[b].x0 + runs[b].x1), 0.f).x);
                return ca > cb;
            });
            for (int oi : order) {
                const Run& r = runs[oi];
                const vec3 p = layout::cellPos(0.5f * (r.x0 + r.x1), (float)y);
                const int side = p.x < 0.f ? 0 : 1;
                const float sg = side ? 1.f : -1.f;
                const int len = r.x1 - r.x0 + 1;
                Flight f;
                f.side = side;
                f.p0 = p;
                f.size0 = vec3((float)len - 0.02f, 0.98f, 0.98f);
                f.col0 = pieceColor(r.type);
                f.len = (float)len;
                f.slideDur = 0.3f;
                const float shift = side ? (5.75f - edgeR) : (edgeL + 5.75f);
                f.slideTo = vec3(p.x + sg * shift, p.y, 0.45f);
                f.high = tetris ? 1.8f : 1.f;
                const float launch = time_ + f.slideDur;
                const int mid = targetModel(side, launch);
                Model* m = model(mid);
                if (!m) continue;
                const int b = pickBrick(*m, r.type, len);
                if (b < 0) continue;
                m->state[b] = 1;
                m->reserved++;
                f.model = mid;
                f.brick = b;
                const float base = (tetris ? 1.5f : 1.15f) + 0.05f * rowK;
                const float arrive = std::max({launch + base, lastArrive_[side] + 0.09f, m->readyAt + 0.3f});
                lastArrive_[side] = arrive;
                f.arcDur = arrive - launch;
                if (f.arcDur > base + 1.2f) {
                    // the next baseplate isn't there yet: it waits above the turntable
                    f.park = true;
                    f.leg1 = base;
                    const int pk = parkCount_[side]++;
                    const float a = pk * 2.1f;
                    f.parkAt = tableCenter(side) + vec3(2.6f * std::cos(a), -TABLE + 0.8f + 0.55f * (pk % 4), 2.6f * std::sin(a));
                }
                f.axis = normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(-0.3f, 1.f), rng_.range(-1.f, 1.f)));
                f.spin = rng_.range(2.5f, 4.5f) * (rng_.chance(0.5f) ? 1.f : -1.f);
                flights_.push_back(f);
            }
        }
        {
            int nb[Game::H][Game::W] = {};
            float np[Game::H][Game::W] = {}, nv[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(np[dst], press_[y], sizeof(press_[y]));
                std::memcpy(nv[dst], pressV_[y], sizeof(pressV_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(press_, np, sizeof(press_));
            std::memcpy(pressV_, nv, sizeof(pressV_));
        }
        if (tetris) {
            // the models glow softly, lift whole and turn once (the gallery's models glow too)
            for (Model& m : models_)
                if (!m.dying) m.glow = std::max(m.glow, m.onTable ? 0.12f : 0.08f);
            liftModels(1.6f);
            extraSpin_[0] += TAU;
            extraSpin_[1] += TAU;
        }
        break;
    }
    case GameEvent::TopOut: {
        // the stack's bricks pop off the board, from the top down, and tumble off the table
        overT_ = 0.f;
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            Debris d;
            d.p = layout::cellPos((float)c.x, (float)c.y) + vec3(0.f, 0.f, -0.5f);
            d.v = vec3(rng_.range(-1.6f, 1.6f), rng_.range(1.0f, 3.5f), rng_.range(2.0f, 4.5f));
            d.axis = normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f)));
            d.size = vec3(0.98f, 0.98f, 0.98f);
            d.col = pieceColor(c.type);
            d.ang = 0.f;
            d.w = rng_.range(2.f, 5.f);
            d.t = 0.f;
            d.delay = 0.35f + 0.06f * (c.y - top) + rng_.range(0.f, 0.08f);
            debris_.push_back(d);
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void BricksUniverse::pushBox(const vec3& c, const vec3& size, const vec3& col, const Quat& q, int mask, float slope,
                             float stack, float mat, float bevel, float fog, float glow) {
    boxes_.push_back({{c.x, c.y, c.z, (float)mask}, {size.x, size.y, size.z, slope}, {col.x, col.y, col.z, fog},
                      {q.x, q.y, q.z, q.w}, {stack, mat, bevel, glow}});
}

void BricksUniverse::pushStud(const vec3& base, float r, float h, const vec3& col, const Quat& q, float stack, float fog,
                              float glow, float mat) {
    studs_.push_back({{base.x, base.y, base.z, 0.f}, {r, h, r, 0.f}, {col.x, col.y, col.z, fog}, {q.x, q.y, q.z, q.w},
                      {stack, mat, 0.f, glow}});
}

void BricksUniverse::buildBoard(const Game& g) {
    const VariantDef& V = VARIANTS[variant_];
    const Quat id;
    const float bev = 0.1f;
    // the board standing on its floor plate
    const vec3 boardCol = lin(V.board);
    pushBox(vec3(0.f, 0.5f * (-10.f + 10.35f), -1.25f), vec3(10.9f, 20.35f, 0.5f), boardCol, id, 0, 0.f, 0.f, 1.f, 0.12f);
    pushBox(vec3(0.f, 0.5f * (TABLE - 10.f), -0.55f), vec3(10.9f, -10.f - TABLE, 1.9f), boardCol * 0.82f, id, 0, 0.f, 0.f,
            1.f, 0.08f);
    // the floor plate's studs in front of the board, under the empty bottom cells
    for (int x = 0; x < Game::W; x++) {
        const vec3 p = layout::cellPos((float)x, (float)(Game::H - 1));
        pushStud(vec3(p.x, -10.f, 0.12f + 0.0f), 0.26f, 0.16f, boardCol * 0.82f, id, 0.f, 0.f, 0.f, 1.f);
        if (g.cell(x, Game::H - 1).type < 0) pushStud(vec3(p.x, -10.f, -0.5f), 0.3f, 0.17f, boardCol * 0.82f, id, 0.f, 0.f, 0.f, 1.f);
    }

    // The stack: each piece one brick (the faces between its cells removed, bevels on its free edges), a stud on each
    // top face left open.
    auto sameAt = [&](int x, int y, int nx, int ny) {
        if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H) return false;
        if (g.cell(nx, ny).type < 0) return false;
        if (std::fabs(g.rowOffset(ny) - g.rowOffset(y)) > 0.02f) return false;
        if (std::fabs(press_[ny][nx] - press_[y][x]) > 0.02f) return false;
        if (ids_[y][x] && ids_[ny][nx]) return ids_[ny][nx] == ids_[y][x];
        return g.cell(nx, ny).type == g.cell(x, y).type;
    };
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, -1, 1, -1, -1, 1, 1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x, y, x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    unsigned char occ[Game::H][Game::W] = {};
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            occ[y][x] = 255;
            const float off = g.rowOffset(y);
            const vec3 p = layout::cellPos((float)x, y - off);
            const float z = press_[y][x];
            const int m = maskOf(x, y, sameAt);
            const vec3 col = pieceColor(c.type);
            pushBox(vec3(p.x, p.y, -0.5f + z), vec3(1.f, 1.f, 1.f), col, id, m, 0.f, 1.f, 0.f, bev);
            const bool open = y == Game::HIDDEN || g.cell(x, y - 1).type < 0 || std::fabs(g.rowOffset(y - 1) - off) > 0.05f;
            if (open) pushStud(vec3(p.x, p.y + 0.5f, -0.5f + z), 0.3f, 0.17f, col, id, 1.f);
        }
    glBindTexture(GL_TEXTURE_2D, occTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Game::W, Game::H, GL_RED, GL_UNSIGNED_BYTE, occ);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // The falling piece: exactly a locked brick, held a little off the board (its shadow tells).
    flat_.clear();
    if (g.hasPiece() && !g.over()) {
        const Piece& pc = g.piece();
        int cells[4][2];
        g.pieceCells(pc, cells);
        auto inPiece = [&](int, int, int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        const float dx = g.pieceVisualX() - pc.x, dy = g.pieceVisualY() - pc.y;
        const vec3 col = pieceColor(pc.type);
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            pushBox(vec3(w.x, w.y, -0.5f + HOLD_Z), vec3(1.f, 1.f, 1.f), col, id, maskOf(c[0], c[1], inPiece), 0.f, 1.f, 0.f, bev);
            if (!inPiece(0, 0, c[0], c[1] - 1)) pushStud(vec3(w.x, w.y + 0.5f, -0.5f + HOLD_Z), 0.3f, 0.17f, col, id, 1.f);
        }
        // The ghost: a fine outline on the board where it will land.
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
            const bool lightBoard = ugl::luma(boardCol) > 0.2f;
            const vec3 ink = lightBoard ? mixDisplay(boardCol, lin(0x1a1a1e), 0.55f) : mixDisplay(boardCol, lin(0xffffff), 0.5f);
            const float a = 0.8f, wdt = 0.055f, in = 0.07f;
            auto seg = [&](float x0, float y0, float x1, float y1) {
                // a thin quad along the segment (in the board's plane, just in front of it)
                const float ex = (x1 - x0), ey = (y1 - y0);
                const float l = std::sqrt(ex * ex + ey * ey);
                const float nx = -ey / l * wdt * 0.5f, ny = ex / l * wdt * 0.5f;
                const float zz = -0.99f;
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

    // Previews: small bricks of the next pieces and the held one.
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
        auto inPiece = [&](int, int, int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        const vec3 col = mixDisplay(pieceColor(type), backdrop(), dim);
        for (auto& c : cells) {
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            pushBox(vec3(px, py, -0.3f * sc), vec3(sc, sc, 0.6f * sc), col, id, maskOf(c[0], c[1], inPiece), 0.f, 0.f, 0.f,
                    bev * sc);
            if (!inPiece(0, 0, c[0], c[1] - 1)) pushStud(vec3(px, py + 0.5f * sc, -0.3f * sc), 0.3f * sc, 0.17f * sc, col, id, 0.f);
        }
    };
    // (drawn after the shadow pass: the previews cast no shadow on the table)
    previewStart_[0] = boxes_.size();
    previewStart_[1] = studs_.size();
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.55f : 0.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 0.f);
    }
    previewEnd_[0] = boxes_.size();
    previewEnd_[1] = studs_.size();
}

// The turntables, the models (placed bricks), the bricks in flight and the falling debris.
void BricksUniverse::buildModels() {
    const VariantDef& V = VARIANTS[variant_];
    const Quat id;
    for (int s = 0; s < 2; s++) {
        const vec3 c = tableCenter(s);
        pushStud(c, TT_R, TT_H, lin(V.turntable), id, 0.f, 0.f, 0.f, 2.f);
    }
    auto studsOf = [&](const vec3& center, const Quat& q, int len, int wid, float h, const vec3& col, float fog, float glow) {
        const Q qq{q.x, q.y, q.z, q.w};
        for (int i = 0; i < len; i++)
            for (int j = 0; j < wid; j++) {
                const vec3 o(((i + 0.5f) - len * 0.5f) * P, h * 0.5f, ((j + 0.5f) - wid * 0.5f) * P);
                pushStud(center + qRot(qq, o), 0.3f * P, 0.2f * P, col, q, 0.f, fog, glow);
            }
    };
    for (const Model& m : models_) {
        const bm::ModelDef& d = bm::all()[m.def];
        if (m.onTable && m.appear <= 0.f) continue;
        // the baseplate (dropping in on a new turntable)
        const float drop = m.onTable ? (1.f - easeOutBack(saturate(m.appear))) * 3.f : 0.f;
        const Q qy = qYaw(m.yaw);
        const Quat qyq{qy.x, qy.y, qy.z, qy.w};
        const vec3 bc = m.pos + vec3(0.f, PLATE * 0.5f + drop, 0.f);
        const vec3 baseCol = d.baseRole < 7 ? lin(V.piece[bm::ROLE_PIECE[d.baseRole]]) : lin(V.neutral[d.baseRole - 7]);
        pushBox(bc, vec3(d.W * P - 0.02f, PLATE - 0.01f, d.D * P - 0.02f), baseCol, qyq, 0, 0.f, 0.f, 0.f, 0.05f, m.fog, m.glow);
        // its studs (those under bricks are hidden inside them)
        for (int x = 0; x < d.W; x++)
            for (int z = 0; z < d.D; z++) {
                const vec3 o((x + 0.5f - d.W * 0.5f) * P, PLATE * 0.5f, (z + 0.5f - d.D * 0.5f) * P);
                pushStud(bc + qRot(qy, o), 0.3f * P, 0.2f * P, baseCol, qyq, 0.f, m.fog, m.glow);
            }
        for (size_t i = 0; i < d.bricks.size(); i++) {
            if (m.state[i] != 2) continue;
            const bm::BrickDef& b = d.bricks[i];
            Quat q;
            const vec3 c = brickWorld(m, (int)i, &q, true);
            const vec3 size = brickSizeWorld(m.def, (int)i);
            const vec3 col = brickColor(m.def, (int)i);
            pushBox(c, size, col, q, 0, b.slope ? 1.f : 0.f, 0.f, 0.f, 0.045f, m.fog, m.glow);
            if (!b.slope) studsOf(c, q, b.len, b.wid, size.y, col, m.fog, m.glow);
        }
    }
    // bricks in flight: out of the row, then an arc to the model, reshaping and repainting on the way
    for (const Flight& f : flights_) {
        Model* m = model(f.model);
        if (!m) continue;
        vec3 pos, size = f.size0, col = f.col0;
        Quat q;
        float slope = 0.f;
        int len = (int)f.len, wid = 1;
        const bm::BrickDef& b = bm::all()[m->def].bricks[f.brick];
        if (f.phase == 0) {
            const float u = saturate(f.t / f.slideDur);
            // fast out of the row, still moving when the arc takes over
            const float e = 0.6f * u * (2.f - u) + 0.4f * u;
            pos = lerp(f.p0, f.slideTo, e);
            pos.z = -0.5f + lerpf(0.f, f.slideTo.z + 0.5f, smoothstepf(0.f, 0.6f, u));
        } else {
            Quat qt;
            const vec3 tgt = brickWorld(*m, f.brick, &qt, true) + vec3(0.f, SNAP_H, 0.f);
            const vec3 side(f.side ? 1.f : -1.f, 0.f, 0.f);
            // the first leg: out of the well to the model, or to a waiting place above its turntable (when the next
            // model's baseplate is not there yet); it leaves with the slide's speed and eases in
            const float leg = f.park ? f.leg1 : f.arcDur;
            const vec3 end1 = f.park ? f.parkAt : tgt;
            const float u = saturate(f.t / leg);
            const float k0 = 0.5f;
            const float e = k0 * u + (3.f - 2.f * k0) * u * u + (k0 - 2.f) * u * u * u;
            const vec3 c1 = f.arcFrom + side * 3.f + vec3(0.f, 3.2f * f.high, -2.5f);
            const vec3 c2 = end1 + vec3(0.f, (f.park ? 1.5f : 4.5f) * f.high, 0.f);
            pos = bezier(f.arcFrom, c1, c2, end1, e);
            const float k = smoothstepf(0.1f, 0.85f, u);
            size = lerp(f.size0, brickSizeWorld(m->def, f.brick), k);
            col = mixDisplay(f.col0, brickColor(m->def, f.brick), smoothstepf(0.3f, 0.75f, u));
            slope = b.slope ? smoothstepf(0.5f, 0.95f, u) : 0.f;
            const Q target{qt.x, qt.y, qt.z, qt.w};
            Q qq = qMul(qSlerp(Q{}, target, e), qAxis(f.axis, f.spin * (1.f - e) * e * 4.f));
            if (k > 0.5f) { len = b.len; wid = b.wid; }
            if (f.park && f.t > leg) {
                // waiting, bobbing gently, then down to its place
                const float descend = 1.0f;
                const float holdEnd = std::max(leg, f.arcDur - descend);
                auto held = [&](float t) { return f.parkAt + vec3(0.f, 0.14f * std::sin((t - leg) * 2.2f), 0.f); };
                if (f.t < holdEnd) pos = held(f.t);
                else {
                    const float v = smoother((f.t - holdEnd) / std::max(0.05f, f.arcDur - holdEnd));
                    const vec3 a = held(holdEnd);
                    pos = bezier(a, a + vec3(0.f, 0.6f, 0.f), tgt + vec3(0.f, 2.2f, 0.f), tgt, v);
                }
                qq = target;
            }
            q = Quat{qq.x, qq.y, qq.z, qq.w};
        }
        pushBox(pos, size, col, q, 0, slope, 0.f, 0.f, 0.05f);
        if (slope <= 0.f) {
            // its studs follow its shape
            const Q qq{q.x, q.y, q.z, q.w};
            const float sx = size.x / std::max(1, len), sz = size.z / std::max(1, wid);
            const float r = 0.3f * std::min(sx, sz), h = 0.2f * std::min(sx, sz);
            for (int i = 0; i < len; i++)
                for (int j = 0; j < wid; j++) {
                    const vec3 o(((i + 0.5f) - len * 0.5f) * sx, size.y * 0.5f, ((j + 0.5f) - wid * 0.5f) * sz);
                    pushStud(pos + qRot(qq, o), r, h, col, q, 0.f);
                }
        }
    }
    for (const Debris& d : debris_) {
        const Q qq = qAxis(d.axis, d.ang);
        const Quat q{qq.x, qq.y, qq.z, qq.w};
        pushBox(d.p, d.size, d.col, q, 0, 0.f, d.delay > 0.f ? 1.f : 0.f, 0.f, 0.075f);
        pushStud(d.p + qRot(qq, vec3(0.f, 0.49f, 0.f)), 0.3f, 0.17f, d.col, q, 0.f);
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float BricksUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void BricksUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void BricksUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha) {
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

void BricksUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    ugl::set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// The lettering in the studio's own ink (graphite on a pale studio, chalk on a dark one); the bonus gauge a ring of
// twelve studs seen from above, the charged ones in the accent.
void BricksUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    const VariantDef& V = VARIANTS[variant_];
    const vec3 bg = backdrop();
    const bool pale = ugl::luma(bg) > 0.25f;
    const vec3 ink = ugl::toSrgb(pale ? lin(0x26262c) : lin(0xf4f1ea)), halo = ugl::toSrgb(pale ? lin(0xf6f3ee) : lin(0x0b0d12));
    const vec3 accent = ugl::toSrgb(pale ? mixDisplay(lin(V.accent), lin(0x000000), 0.15f) : lin(V.accent));
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);

    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float q[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                   {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : q) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }

    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = Game::BONUS_LINES;
        auto disc = [&](float cx, float cy, float rad, const vec3& col, float al) {
            const int S = 20;
            for (int k = 0; k < S; k++) {
                const float a0 = TAU * k / S, a1 = TAU * (k + 1) / S;
                const float v[3][6] = {{cx, cy, col.x, col.y, col.z, al},
                                       {cx + std::cos(a0) * rad, cy + std::sin(a0) * rad, col.x, col.y, col.z, al},
                                       {cx + std::cos(a1) * rad, cy + std::sin(a1) * rad, col.x, col.y, col.z, al}};
                for (auto& q : v) shapes_.insert(shapes_.end(), q, q + 6);
            }
        };
        for (int i = 0; i < N; i++) {
            const float a = TAU * i / N;
            const float cx = c.x + std::sin(a) * r, cy = c.y - std::cos(a) * r;
            const float part = saturate(fill * N - i);
            const float rad = r * 0.17f;
            const float al = (g.bonusReady() ? 0.75f : 0.4f) * vis * (1.f - 0.5f * dimT_);
            // a stud: its rim, then its top (charged: the accent)
            disc(cx, cy, rad, halo, al * 0.6f);
            disc(cx, cy, rad * 0.78f, ink, al * (1.f - part) * 0.7f);
            disc(cx, cy, rad * 0.78f, accent, part * 0.95f * vis * (1.f - 0.5f * dimT_));
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
            place(fontMed_, upper(h.text), base * 0.72f, base * 0.18f, base * 0.1f);
            drawText(fontMed_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f));
            continue;
        case HUD_SECTION:
            place(fontMed_, upper(h.text), base * 0.9f, base * 0.16f);
            drawText(fontMed_, q, accent, halo, saturate(0.25f + h.alpha * 1.7f));
            continue;
        case HUD_TITLE:
            place(fontLight_, h.text, base * 1.1f, 0.f, -base * 0.1f);
            drawText(fontLight_, q, ink, halo, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontLight_, h.text, base * 0.72f, base * 0.06f);
            drawText(fontLight_, q, ink, halo, saturate(alpha * 1.2f) * 0.75f);
            continue;
        case HUD_LABEL:
            place(fontMed_, upper(h.text), base * 0.62f, base * 0.24f, base * 0.06f);
            drawText(fontMed_, q, ink, halo, saturate(alpha * 1.2f) * 0.72f);
            continue;
        case HUD_VALUE:
            place(fontMed_, h.text, base * 0.98f, base * 0.03f);
            drawText(fontMed_, q, ink, halo, alpha);
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
            place(fontMed_, upper(h.text), base * 1.1f, base * 0.14f, -base * 0.1f);
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

void BricksUniverse::drawSolids(const mat4& vp, bool shadow) {
    const GLuint prog = shadow ? progShadow_ : progBrick_;
    glUseProgram(prog);
    setMat(prog, "uVP", vp);
    // the previews are left out of the shadow map
    auto draw = [&](std::vector<Inst>& v, GLuint vao, GLuint vbo, int verts, int stud, size_t p0, size_t p1) {
        if (v.empty()) return;
        set1i(prog, "uStud", stud);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        if (!shadow) glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Inst), v.data(), GL_STREAM_DRAW);
        else {
            std::vector<Inst> w;
            w.reserve(v.size());
            w.insert(w.end(), v.begin(), v.begin() + p0);
            w.insert(w.end(), v.begin() + p1, v.end());
            glBufferData(GL_ARRAY_BUFFER, w.size() * sizeof(Inst), w.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLES, 0, verts, (GLsizei)w.size());
            return;
        }
        glDrawArraysInstanced(GL_TRIANGLES, 0, verts, (GLsizei)v.size());
    };
    draw(boxes_, boxVao_, instVbo_, 36, 0, previewStart_[0], previewEnd_[0]);
    draw(studs_, studVao_, studInstVbo_, studVerts_, 1, previewStart_[1], previewEnd_[1]);
}

void BricksUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    time_ += f.paused ? 0.f : dt;
    worldTime_ += wdt;
    hasBeat_ = music.hasSong;
    if (music.hasSong) beat_ = music.beat;
    else beat_ = worldTime_ * 2.0;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    swell_ = approach(swell_, 0.f, 0.4f, dt);
    if (!dropMode_) dropRelease_ = std::max(0.f, dropRelease_ - dt / 1.1f); // home, eased per brick
    // the lock's click: a stiff spring onto the board
    if (!f.paused)
        for (int y = 0; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                float& z = press_[y][x];
                float& v = pressV_[y][x];
                if (z == 0.f && v == 0.f) continue;
                const float a = -900.f * z - 34.f * v;
                v += a * dt;
                z += v * dt;
                if (std::fabs(z) < 0.0005f && std::fabs(v) < 0.01f) { z = 0.f; v = 0.f; }
            }

    // ---- The drop's choreography, on the beat grid: apart over two beats, hovering two, back two, still two.
    {
        const double rel = beat_ - dropBeat0_;
        float sched = 0.f;
        if (dropRelease_ > 0.f && rel >= 0.0) {
            const int cycle = (int)std::floor(rel / 8.0);
            const double in = rel - cycle * 8.0;
            const int ph = (int)std::floor(in / 2.0);
            const float fr = (float)((in - ph * 2.0) / 2.0);
            if (cycle != dropCycle_) {
                dropCycle_ = cycle;
                if (dropS_ <= 0.001f) dropForm_ = cycle % 3;
            }
            sched = ph == 0 ? fr : (ph == 1 ? 1.f : (ph == 2 ? 1.f - fr : 0.f));
        }
        dropS_ = std::min(sched, dropRelease_ * 1.f);
        if (dropS_ < 0.f) dropS_ = 0.f;
    }

    // ---- The beat grid (every two beats): from mid on, the models lift gently, whole; higher at a peak.
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
        // mid and peak: the models lift gently, whole, every two beats (never during the drop's figures)
        if (tick && overT_ < 0.f && dropRelease_ <= 0.f && lv_ > 0.6f) liftModels(0.5f + 0.5f * wp);
    }
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    // a peak's kicks: a soft glow on the models (local, small)
    if (!f.paused && overT_ < 0.f && wp > 0.5f && kick > 0.45f && kickPrev_ <= 0.3f)
        for (int s = 0; s < 2; s++)
            if (Model* m = model(cur_[s])) m->glow = std::max(m->glow, 0.05f * kick);
    for (int s = 0; s < 2; s++) liftT_[s] += wdt;
    kickPrev_ = kick;

    // the Tetris' extra turn, eased out
    for (int s = 0; s < 2; s++) {
        const float before = extraSpin_[s];
        extraSpin_[s] = approach(extraSpin_[s], 0.f, 1.4f, dt);
        tableYaw_[s] += (s ? -1.f : 1.f) * (before - extraSpin_[s]);
    }
    updateModels(dt, wdt);
    updateFlights(dt, wdt);
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- Geometry.
    boxes_.clear();
    studs_.clear();
    buildBoard(game);
    buildModels();

    // ---- Light: a soft key from the upper left (an evening light from the right in slow motion).
    const VariantDef& V = VARIANTS[variant_];
    const vec3 Lday = normalize(vec3(-0.24f, 0.72f, 0.66f)), Leve = normalize(vec3(0.72f, 0.42f, 0.6f));
    const vec3 L = normalize(lerp(Lday, Leve, smoothstepf(0.f, 1.f, slow_)));
    const float keyK = V.key * (0.9f + 0.05f * wm + 0.08f * wp + 0.05f * swell_);
    const vec3 keyCol = lerp(vec3(1.0f, 0.96f, 0.9f), vec3(1.0f, 0.72f, 0.45f), slow_) * keyK * 1.05f;
    const vec3 tableNear = lin(V.tableNear), tableFar = lin(V.tableFar), wallTop = lin(V.wallTop);
    // the light's pool on the table: the variant's warmth, warmer and wider in the drop (one slow swell)
    const vec3 poolCol = lin(V.pool) + lin(V.dropGlow) * (0.03f * dropRelease_ + 0.03f * swell_ + 0.01f * wp);

    // the shadow map: the key light's orthographic view around the well and the turntables
    {
        const vec3 center(0.f, 0.f, -8.f);
        const mat4 lv = mat4::lookAt(center + L * 60.f, center, vec3(0, 1, 0));
        float mn[3] = {1e9f, 1e9f, 1e9f}, mx[3] = {-1e9f, -1e9f, -1e9f};
        const float xs[2] = {-31.f, 31.f}, ys[2] = {TABLE - 0.5f, 15.f}, zs[2] = {-22.f, 5.f};
        for (float x : xs)
            for (float y : ys)
                for (float z : zs) {
                    const vec4 p = lv * vec4(x, y, z, 1.f);
                    const float c[3] = {p.x, p.y, p.z};
                    for (int i = 0; i < 3; i++) { mn[i] = std::min(mn[i], c[i]); mx[i] = std::max(mx[i], c[i]); }
                }
        lightVp_ = mat4::ortho(mn[0], mx[0], mn[1], mx[1], -mx[2] - 5.f, -mn[2] + 5.f) * lv;
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glViewport(0, 0, 4096, 4096);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 3.f);
        drawSolids(lightVp_, true);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // ---- The studio, then the solids (multisampled).
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glUseProgram(progStudio_);
    common(progStudio_);
    setMat(progStudio_, "uInvVP", invVp_);
    setMat(progStudio_, "uLightVP", lightVp_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    set1i(progStudio_, "uShadow", 1);
    set1f(progStudio_, "uShadowK", 0.6f);
    set1f(progStudio_, "uGrain", 0.018f);
    {
        // contact shades under the gallery's models (beyond the shadow map) and the turntables
        float blobs[24 * 4] = {};
        int nb = 0;
        for (int s = 0; s < 2 && nb < 24; s++) {
            const vec3 c = tableCenter(s);
            blobs[nb * 4 + 0] = c.x; blobs[nb * 4 + 1] = c.z; blobs[nb * 4 + 2] = TT_R * 1.15f; blobs[nb * 4 + 3] = 0.35f;
            nb++;
        }
        for (const Model& m : models_) {
            if (m.onTable || nb >= 24) continue;
            const bm::ModelDef& d = bm::all()[m.def];
            const float lift = saturate(1.f - (m.pos.y - TABLE) / 3.f);
            blobs[nb * 4 + 0] = m.pos.x; blobs[nb * 4 + 1] = m.pos.z;
            blobs[nb * 4 + 2] = 0.6f * std::max(d.W, d.D) * P; blobs[nb * 4 + 3] = 0.45f * lift * (1.f - m.fog);
            nb++;
        }
        glUniform4fv(ugl::U(progStudio_, "uBlobs"), 24, blobs);
        set1i(progStudio_, "uBlobCount", nb);
    }
    ugl::drawFullscreen();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUseProgram(progBrick_);
    common(progBrick_);
    setMat(progBrick_, "uLightVP", lightVp_);
    set1i(progBrick_, "uShadow", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, occTex_);
    set1i(progBrick_, "uOcc", 2);
    glActiveTexture(GL_TEXTURE0);
    const float ambK = V.amb * (1.f - 0.25f * slow_);
    set3f(progBrick_, "uAmbTop", mixDisplay(wallTop, vec3(0.75f, 0.8f, 0.9f), 0.35f) * (0.42f * ambK));
    set3f(progBrick_, "uAmbBot", tableNear * (0.22f * ambK));
    set3f(progBrick_, "uFillDir", vec3(0.6f, 0.2f, 0.8f));
    set3f(progBrick_, "uFillCol", vec3(0.75f, 0.82f, 1.0f) * (0.1f * ambK));
    set1f(progBrick_, "uSpec", 1.f);
    set1f(progBrick_, "uWarm", 0.35f);
    set1f(progBrick_, "uShadowK", 0.9f);
    drawSolids(vp_, false);
    glDisable(GL_CULL_FACE);

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
    set3f(progFinal_, "uSlowTint", vec3(1.0f, 0.86f, 0.74f));
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

const UniverseEntry& universe_bricks() {
    static const UniverseEntry e{
        "BRICKS", "glossy toy bricks in a photo studio: every cleared row flies off as bricks that build models on turntables",
        [] { return std::unique_ptr<Universe>(new BricksUniverse()); },
        // Playful, bright, rhythmic songs suit the toy studio best; it fits most songs.
        [](const SongLook& l) { return 0.8f + 0.25f * l.brightness + 0.15f * l.density; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
