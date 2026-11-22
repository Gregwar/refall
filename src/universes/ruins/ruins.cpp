// RUINS: time running backwards, behind the well, in 3D. A song starts in overgrown ruins (broken columns and the
// temple in plain sight among trees, roots, vines and moss); with the lines the jungle recedes (its trees shrink
// back, the roots withdraw, the undergrowth sinks, the moss leaves the stones) and clears the ruins; then time rewinds and the ruins
// repair themselves: fallen column tops lift from the ground and set back onto their stumps, tumbled blocks fly back
// up into the walls, missing stones re-form out of golden dust, towers regain their tiers, edges sharpen, carvings
// come back crisp, lichen and streaks fade; by scene completion (50 paced lines) the temple stands in its prime,
// painted, its braziers lit, and holds. The camera drifts slowly around it, keeping it beside the well. The pieces
// are pale stone blocks, one per cell, each with a frame and a relief glyph cut in, a deeper pigment in the cuts (I
// three bars, O a sun disc, T a cross, S a spiral, Z two waves, J a stepped pyramid, L an eye); a cleared row crumbles behind the well and sends dust to the next
// repairs. The drop turns the sun to golden hour for the whole drop section (and the dust glows).
// Before a song the camera comes in from a step back as the morning mist lifts off the ruins; after it the camera
// pulls back as the mist settles back and the sun weakens.
//
// The world is meshes (world.cpp), built per variant on the CPU from the ruin's parameters and drawn instanced at the
// full resolution, 4x multisampled (up to 1440 lines): the temple (a Maya pyramid, a Khmer temple mountain with a
// strangler fig, a Doric temple), the avenue, the trunks and the ground; the repairs are instance transforms (parts
// flying back into place) and dissolves (stones and tiers re-forming out of golden dust), the missing blocks holes
// seen into the walls (shaders.hpp), the wear, the jungle's cover and the prime uniforms of the stone's material. The
// sun casts a shadow map, dappled by the canopy; the far jungle's walls and the lianas are cards with leafy masks
// (alpha to coverage), the undergrowth, the canopy overhead and the crowns instanced clumps of leafy blobs (alpha to
// coverage too). Only the
// soft terms are at a reduced resolution: the mist and light shafts along each ray to the depth, and a little
// ambient occlusion, upsampled with the depth.
#include "universes/ruins/ruins.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/ruins/shaders.hpp"

namespace {

// Marcellus (labels, numbers) and Cormorant Garamond Italic (the song, the variant, the notices), SIL Open Font
// License, see OFL-Marcellus.txt and OFL-CormorantGaramond.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/ruins/Marcellus-Regular.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/ruins/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = ruins_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;
using Palette = RuinsUniverse::Palette;

GLuint compile(const char* vs, const char* fs, const char* name, bool scene = false) {
    const std::string common = std::string(shaders::COMMON_GLSL) + (scene ? shaders::SCENE_GLSL : "");
    std::string f(fs);
    f.insert(f.find('\n') + 1, common);
    std::string v(vs);
    v.insert(v.find('\n') + 1, common);
    return ugl::program(v.c_str(), f.c_str(), name);
}
constexpr int SHADOW_SIZE = 3072;
constexpr float NEAR = 0.4f, FAR = 900.f;

vec2 V(float x, float y) { return vec2{x, y}; }
// A unit icosphere, subdivided n times (its vertices shared): the bushes' blobs.
void icosphere(int n, std::vector<vec3>& v, std::vector<unsigned short>& idx) {
    const float t = (1.f + std::sqrt(5.f)) * 0.5f;
    v = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
         {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    for (vec3& p : v) p = normalize(p);
    idx = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6,
           7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
    for (int k = 0; k < n; k++) {
        std::map<std::pair<int, int>, int> mid;
        auto m = [&](int a, int b) {
            const auto key = std::make_pair(std::min(a, b), std::max(a, b));
            const auto it = mid.find(key);
            if (it != mid.end()) return it->second;
            v.push_back(normalize(v[a] + v[b]));
            return mid[key] = (int)v.size() - 1;
        };
        std::vector<unsigned short> sub;
        for (size_t i = 0; i < idx.size(); i += 3) {
            const int a = idx[i], b = idx[i + 1], c = idx[i + 2], ab = m(a, b), bc = m(b, c), ca = m(c, a);
            for (int j : {a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca}) sub.push_back((unsigned short)j);
        }
        idx.swap(sub);
    }
}
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant (sRGB): its temple (0 Maya, 1 Khmer, 2 Greek), its stone, its jungle, its light, its seven blocks
// (sandstone, ochre, jade, slate blue, terracotta, white marble, rose granite, each tuned to the place).
struct VariantDef {
    const char* name;
    int kind;
    unsigned stone, stone2, moss, moss2, leaf, leafLit, bark, ground, skyZ, skyH, fog, sun, amb;
    unsigned ink, accent, veil;
    unsigned piece[7];
    float fogK, gap;
};
const VariantDef VARIANTS[] = {
    // MAYA: a grey limestone pyramid in a lush green rainforest
    {"MAYA", 0, 0xb4ab96, 0x625d50, 0x6f8c32, 0x2f4a1a, 0x1f3f1c, 0xb6d84a, 0x5c5248, 0x4a3f2a, 0x6c9cc8, 0xd6e6dc,
     0xc4ccc2, 0xfff0d2, 0x7a92a8,
     0xf6eedc, 0x9ad8a6, 0x0a0f0a,
     {0xe6c890, 0xdc9c40, 0x58b088, 0x6e92c8, 0xcc6a4c, 0xeee8dc, 0xdc8e9a}, 0.006f, 0.1f},
    // KHMER: a sandstone temple mountain, a strangler fig pouring its roots over it, humid mist
    {"KHMER", 1, 0xa89a80, 0x4e4a40, 0x7a8a3a, 0x3a4a22, 0x22381c, 0xc0d050, 0xb2aa9c, 0x544730, 0x86a2b4, 0xe2e4d2,
     0xcccbb8, 0xfff0d4, 0x8494a0,
     0xf8f0dc, 0xf2bc66, 0x0d0e09,
     {0xdec28e, 0xd8963a, 0x50ac92, 0x7088bc, 0xc46248, 0xeae4d6, 0xd48ca0}, 0.008f, 0.12f},
    // GREEK: a white marble temple under umbrella pines and cypresses, a Mediterranean blue sky
    {"GREEK", 2, 0xdcd4c4, 0x8a8274, 0x7e8a3c, 0x4a5326, 0x2c4424, 0xb0c060, 0x5e4c3c, 0x8a7656, 0x4c84c6, 0xdce6ee,
     0xd0dae4, 0xfff4e0, 0x8aa2be,
     0xfaf6ee, 0xe8d08c, 0x0b0d10,
     {0xeccf9a, 0xe4a448, 0x62ba96, 0x789ad4, 0xd4705a, 0xf6f2ea, 0xe49aa8}, 0.004f, 0.2f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[wrapV(v)];
    auto L = [](unsigned x) { return ugl::toLinear(hex(x)); };
    Palette p;
    p.stone = L(d.stone); p.stone2 = L(d.stone2); p.moss = L(d.moss); p.moss2 = L(d.moss2); p.leaf = L(d.leaf);
    p.leafLit = L(d.leafLit); p.bark = L(d.bark); p.ground = L(d.ground); p.skyZ = L(d.skyZ); p.skyH = L(d.skyH);
    p.fog = L(d.fog); p.sun = L(d.sun); p.amb = L(d.amb); p.ink = L(d.ink); p.accent = L(d.accent); p.veil = L(d.veil);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    vec3* pa = &a.stone;
    const vec3* pb = &b.stone;
    for (int i = 0; i < (int)(sizeof(Palette) / sizeof(vec3)); i++) pa[i] = lerp(pa[i], pb[i], k);
}

constexpr float CLEAR_T = 0.5f; // a cleared block crumbles away in this long (real time)

// Test hooks: REFALL_RUINS_LINES=N adds N lines to the rewind, REFALL_GROW_RATE=R adds R lines a second.
int extraLines(double t) {
    static const int n = std::getenv("REFALL_RUINS_LINES") ? std::atoi(std::getenv("REFALL_RUINS_LINES")) : 0;
    static const double rate = std::getenv("REFALL_GROW_RATE") ? std::atof(std::getenv("REFALL_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool RuinsUniverse::init(int width, int height) {
    progMesh_ = compile(shaders::MESH_VS, shaders::MESH_FS, "ruins mesh", true);
    progShadow_ = compile(shaders::MESH_VS, shaders::SHADOW_FS, "ruins shadow", true);
    progSky_ = compile(ugl::FULLSCREEN_VS, shaders::SKY_FS, "ruins sky", true);
    progFol_ = compile(shaders::FOL_VS, shaders::FOL_FS, "ruins foliage", true);
    progVol_ = compile(ugl::FULLSCREEN_VS, shaders::VOL_FS, "ruins mist", true);
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "ruins comp", true);
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "ruins sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "ruins veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "ruins cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "ruins shape");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "ruins post");
    progBush_ = compile(shaders::BUSH_VS, shaders::BUSH_FS, "ruins bush", true);
    progBushShadow_ = compile(shaders::BUSH_VS, shaders::BUSH_SHADOW_FS, "ruins bush shadow", true);

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

    // ---- the meshes: one vertex buffer per scene (and one for the fig's roots), the instances
    glGenBuffers(1, &instVbo_);
    auto meshVao = [&](GLuint& vao, GLuint& vbo) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ruins::Vtx), (void*)offsetof(ruins::Vtx, p));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ruins::Vtx), (void*)offsetof(ruins::Vtx, n));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ruins::Vtx), (void*)offsetof(ruins::Vtx, a));
        glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
        for (int i = 0; i < 6; i++) {
            glEnableVertexAttribArray(3 + i);
            glVertexAttribDivisor(3 + i, 1);
        }
    };
    meshVao(meshVao_, meshVbo_);
    meshVao(figVao_, figVbo_);
    glGenVertexArrays(1, &folVao_);
    glGenBuffers(1, &folVbo_);
    glBindVertexArray(folVao_);
    glBindBuffer(GL_ARRAY_BUFFER, folVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glBindVertexArray(0);
    {
        std::vector<vec3> ico;
        std::vector<unsigned short> idx;
        icosphere(2, ico, idx);
        bushIdx_ = (int)idx.size();
        glGenVertexArrays(1, &bushVao_);
        glGenBuffers(1, &bushVbo_);
        glGenBuffers(1, &bushIbo_);
        glGenBuffers(1, &bushInstVbo_);
        glBindVertexArray(bushVao_);
        glBindBuffer(GL_ARRAY_BUFFER, bushVbo_);
        glBufferData(GL_ARRAY_BUFFER, ico.size() * sizeof(vec3), ico.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bushIbo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned short), idx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, nullptr);
        for (int i = 1; i <= 3; i++) {
            glEnableVertexAttribArray(i);
            glVertexAttribDivisor(i, 1);
        }
        glBindVertexArray(0);
    }

    // ---- MAYA's corner stones' dissolves (see ruins::Scene::cornerTex)
    glGenTextures(1, &cornerTex_);
    glBindTexture(GL_TEXTURE_2D, cornerTex_);
    {
        const unsigned char z[ruins::Scene::CORNER_W] = {};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ruins::Scene::CORNER_W, 1, 0, GL_RED, GL_UNSIGNED_BYTE, z);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // ---- the sun's shadow map
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
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
    GLint maxS = 4;
    glGetIntegerv(GL_MAX_SAMPLES, &maxS);
    samples_ = std::max(1, std::min(4, (int)maxS));

    pal_ = paletteOf(0);

    // the butterflies: a few, each with its home beside the well
    Rng r(0xB077);
    for (int i = 0; i < 4; i++) {
        Fly f;
        const float side = (i % 2) ? 1.f : -1.f;
        f.home = V(side * r.range(12.f, 16.5f), r.range(-8.5f, -1.f));
        f.p = f.home;
        f.phase = r.range(0.f, TAU);
        f.seed = r.uniform();
        f.hue = (float)i;
        flies_.push_back(f);
    }
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void RuinsUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * (float)width_ / height_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    // the world: multisampled at the full resolution (up to 1440 lines), resolved with its depth
    msH_ = std::max(180, std::min(1440, height_));
    msW_ = std::max(320, (int)std::lround((float)msH_ * width_ / height_));
    if (!msFbo_) { glGenFramebuffers(1, &msFbo_); glGenRenderbuffers(1, &msColor_); glGenRenderbuffers(1, &msDepth_); }
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_RGBA16F, msW_, msH_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_DEPTH_COMPONENT32F, msW_, msH_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "[ruins] multisampled framebuffer incomplete (%dx%d x%d)\n", msW_, msH_, samples_);
    worldT_.create(msW_, msH_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    auto tex = [](GLuint& t, int w, int h, GLenum internal, GLenum format, GLenum type, GLenum filter) {
        if (!t) glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    tex(worldDepth_, msW_, msH_, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, worldT_.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, worldDepth_, 0);
    // the mist, the shafts and the occlusion: at half the resolution (at most 540 lines)
    const int vh = std::max(90, std::min(540, msH_ / 2));
    const int vw = std::max(160, (int)std::lround((float)vh * width_ / height_));
    volT_.create(vw, vh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false, GL_NEAREST);
    tex(auxTex_, vw, vh, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, volT_.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, auxTex_, 0);
    {
        const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, bufs);
    }
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    aspect_ = (float)width_ / height_;
    updateCamera();
}

// The drift: a slow orbit around the temple's court (about 0.3 rad either way over ~4 minutes), a slow dolly in and
// out, the eye's height breathing a little; it looks a little up, at the temple, a little ahead of its swing.
void RuinsUniverse::updateCamera() {
    const float t = camPhase_;
    const float ang = side_ * 0.12f + 0.24f * std::sin(t * 0.026f) + 0.06f * std::sin(t * 0.017f + 1.3f);
    const float R = 64.f + 5.f * std::sin(t * 0.019f + 0.7f);
    const vec3 C(0.f, 0.f, -50.f);
    ovEye_ = C + vec3(std::sin(ang) * R, 3.6f + 1.0f * std::sin(t * 0.023f + 2.f), std::cos(ang) * R);
    // the temple beside the well (on this song's side), not hidden behind it
    const vec3 toC = normalize(vec3(C.x - ovEye_.x, 0.f, C.z - ovEye_.z));
    const vec3 r0 = normalize(cross(toC, vec3(0.f, 1.f, 0.f)));
    ovTgt_ = C + vec3(0.f, 10.5f, 0.f) - r0 * (side_ * 21.f);
    tanH_ = 0.43f;
    if (!camInit_) {
        camEye_ = goalEye_ = ovEye_;
        camTgt_ = goalTgt_ = ovTgt_;
        eyeV_ = tgtV_ = vec3(0.f, 0.f, 0.f);
        stepCamera(0.f);
    }
}

// The camera glides to its goal (the overview, or the repair to watch) on critically damped springs: slow, smooth,
// no jumps.
void RuinsUniverse::stepCamera(float dt) {
    dt = std::min(dt, 0.05f);
    const float w = 1.0f;
    eyeV_ = eyeV_ + ((goalEye_ - camEye_) * (w * w) - eyeV_ * (2.f * w)) * dt;
    tgtV_ = tgtV_ + ((goalTgt_ - camTgt_) * (w * w) - tgtV_ * (2.f * w)) * dt;
    camEye_ = camEye_ + eyeV_ * dt;
    camTgt_ = camTgt_ + tgtV_ * dt;
    eye_ = camEye_;
    fwd_ = normalize(camTgt_ - camEye_);
    right_ = normalize(cross(fwd_, vec3(0.f, 1.f, 0.f)));
    up_ = cross(right_, fwd_);
}

// A repair framed beside the well: the camera a little in front of it (in the avenue for a column, outside the
// colonnade for the temple), turned so the repair sits in the open band beside the well, at its height.
void RuinsUniverse::focusPose(const Ev& e, vec3& eye, vec3& tgt) const {
    const vec3 P = e.focus;
    const float s = e.side;
    const float yaw = e.kind == 2 ? s * 0.12f : -s * 0.5f;
    eye = P + vec3(std::sin(yaw) * e.dist, 0.f, std::cos(yaw) * e.dist);
    eye.y = e.kind == 2 ? std::max(3.2f, P.y * 0.4f) : 2.6f;
    vec3 vh(P.x - eye.x, 0.f, P.z - eye.z);
    const float lh = std::max(1.f, length(vh));
    vh = vh * (1.f / lh);
    const vec3 r = normalize(cross(vh, vec3(0.f, 1.f, 0.f)));
    const float th = std::atan(0.74f * tanH_ * aspect_);
    const vec3 fdir = vh * std::cos(th) - r * (std::sin(th) * s);
    tgt = eye + fdir * lh + vec3(0.f, P.y - eye.y, 0.f);
}

using ruins::ghash; // (GLSL's hash: which columns are broken)

// The plan: first the avenue's columns (down this song's side, back up the other: each broken one's top, then the
// block beside it), then the temple in eight steps through its parts (as shaders.hpp sequences them on uTemple).
void RuinsUniverse::buildPlan() {
    plan_.clear();
    const int kind = VARIANTS[wrapV(variant_)].kind;
    const float H = kind == 0 ? 5.4f : (kind == 1 ? 4.8f : 7.6f);
    for (int si = 0; si < 2; si++)
        for (int jj = 0; jj < 4; jj++) {
            const int j = si == 0 ? jj : 3 - jj;
            const float sd = (si == 0) == (side_ > 0.f) ? 1.f : -1.f;
            const float h0 = ghash(j * 3.1f + 0.7f, sd * 1.7f + 2.f), h1 = ghash(j * 1.3f + 5.1f, sd * 3.3f + 0.4f);
            const int ci = j * 2 + (sd > 0.f ? 1 : 0);
            const bool broken = h0 < 0.62f, block = h1 > 0.45f;
            const vec3 base(sd * 7.5f, 0.75f, -3.5f - 7.f * j);
            if (broken || block) plan_.push_back({0, ci, broken ? 0.f : 0.55f, 1.f, base + vec3(0.f, H * 0.6f, 0.f), H * 2.0f + 1.f, sd});
        }
    struct Seg { float end; vec3 p; float dist; };
    static const Seg MAYA[] = {{0.5f, vec3(15.f, 6.f, -33.f), 26.f}, {1.f, vec3(0.f, 25.f, -51.f), 34.f}};
    static const Seg KHMER[] = {{0.33f, vec3(12.f, 7.f, -41.f), 24.f}, {0.55f, vec3(15.f, 13.f, -53.f), 30.f},
                                {0.78f, vec3(-15.f, 13.f, -53.f), 30.f}, {1.f, vec3(0.f, 20.f, -58.f), 38.f}};
    static const Seg GREEK[] = {{0.25f, vec3(0.f, 9.f, -53.f), 28.f}, {0.5f, vec3(8.f, 7.f, -44.f), 24.f},
                                {0.75f, vec3(5.f, 14.f, -44.f), 26.f}, {1.f, vec3(3.f, 16.f, -44.f), 28.f}};
    const Seg* segs = kind == 0 ? MAYA : (kind == 1 ? KHMER : GREEK);
    const int ns = kind == 0 ? 2 : 4;
    for (int k = 0; k < 8; k++) {
        const float a = k / 8.f, b = (k + 1) / 8.f, mid = (a + b) * 0.5f;
        int si = 0;
        while (si < ns - 1 && mid > segs[si].end) si++;
        const vec3 p = segs[si].p;
        plan_.push_back({2, k, a, b, p, segs[si].dist, p.x > 0.5f ? 1.f : (p.x < -0.5f ? -1.f : side_)});
    }
}

// the repairs owed at L (50-line scale): spread over 12 .. 45
int RuinsUniverse::owedEvents(float L) const {
    const int n = (int)plan_.size();
    if (L < 12.f || n == 0) return 0;
    return std::min(n, (int)((L - 12.f) / (33.f / n)) + 1);
}

void RuinsUniverse::applyEv(const Ev& e, float t) {
    const float v = lerpf(e.from, e.to, t);
    if (e.kind == 2) templeRep_ = std::max(templeRep_, v);
    else colRep_[e.idx] = std::max(colRep_[e.idx], v);
}

// The arc, on the 50-line scale: the jungle recedes (1..12); the repairs (12..45, one event at a time, see the
// plan) drive the columns, the temple, the missing blocks (re-forming with the temple) and the wear; the prime
// (43..50: paint, braziers) once the last repair is done.
void RuinsUniverse::updateRewind(float L) {
    jungle_ = 0.4f * (1.f - smoothstepf(1.f, 12.f, L)); // (overgrown, never a wall: the ruins seen from the start)
    for (int i = 0; i < 8; i++) col_[i] = colRep_[i];
    temple_ = templeRep_;
    broken_ = 0.07f * (1.f - templeRep_);
    float done = 0.f;
    for (int i = 0; i < 8; i++) done += colRep_[i];
    done = (done / 8.f + templeRep_ * 2.f) / 3.f;
    wear_ = 1.f - smoothstepf(0.f, 1.f, done);
    const bool finished = next_ >= (int)plan_.size() && active_ < 0;
    prime_ = finished ? smoothstepf(43.f, 50.f, L) : std::min(prime_, smoothstepf(43.f, 50.f, L));
}

bool RuinsUniverse::toBoard(const vec3& w, vec2& b) const {
    const vec3 v = w - eye_;
    const float z = dot(v, fwd_);
    if (z < 0.5f) return false;
    const float nx = dot(v, right_) / (z * tanH_ * aspect_), ny = dot(v, up_) / (z * tanH_);
    b = V(viewC_.x + nx * viewH_.x, viewC_.y + ny * viewH_.y);
    return std::fabs(nx) < 1.05f && std::fabs(ny) < 1.05f;
}

// golden dust converging on a world point (a stone re-forming), behind the well
void RuinsUniverse::dustAt(const vec3& w, int n) {
    vec2 b;
    if (!toBoard(w, b) || swirls_.size() > 400) return;
    const float z = std::max(5.f, dot(w - eye_, fwd_));
    const float k = viewH_.y / (z * tanH_); // board units per world unit there
    for (int i = 0; i < n; i++) {
        Swirl s;
        s.c = b;
        s.age = 0.f;
        s.age = -rng_.range(0.f, 0.25f);
        s.life = rng_.range(1.1f, 1.8f);
        s.ang = rng_.range(0.f, TAU);
        s.rad = rng_.range(0.8f, 2.4f) * k;
        s.size = rng_.range(0.06f, 0.13f) * std::clamp(k * 2.5f, 0.7f, 1.8f);
        s.seed = rng_.uniform();
        swirls_.push_back(s);
    }
}

RuinsUniverse::~RuinsUniverse() {
    for (ugl::Target* t : {&worldT_, &volT_, &sceneT_}) t->destroy();
    for (GLuint p : {progMesh_, progShadow_, progSky_, progFol_, progVol_, progComp_, progSprite_, progVeil_, progCell_, progShape_, progPost_,
                     progBush_, progBushShadow_})
        glDeleteProgram(p);
    for (GLuint v : {spriteVao_, cellVao_, shapeVao_, meshVao_, figVao_, folVao_, bushVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, cellVbo_, shapeVbo_, meshVbo_, figVbo_, instVbo_, folVbo_, bushVbo_, bushIbo_, bushInstVbo_})
        glDeleteBuffers(1, &b);
    for (GLuint t : {worldDepth_, auxTex_, shadowTex_, cornerTex_}) glDeleteTextures(1, &t);
    for (GLuint fb : {msFbo_, shadowFbo_}) glDeleteFramebuffers(1, &fb);
    for (GLuint rb : {msColor_, msDepth_}) glDeleteRenderbuffers(1, &rb);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void RuinsUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.brightness > 0.6f && t.airWeight > 0.5f) v = 2;           // bright, airy songs: the marble
        else if (t.hasSong && t.bassWeight > 0.6f && (t.minor || t.dynamics > 0.5f)) v = 1; // deep ones: the fig's temple
        else {
            static const int BY_HUE[] = {0, 1, 0, 2, 0, 1};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 2)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x2B1E5ull);
    // each song starts the drift at its own place along the orbit
    if (seconds <= 0.f) {
        palSet_ = false;
        camPhase_ = (float)((seed_ >> 20) % 1000) * 0.24f;
        side_ = ((seed_ >> 12) & 1) ? 1.f : -1.f;
    }
}

void RuinsUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

void RuinsUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
    releaseBirds(9, 0);
}

void RuinsUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

vec2 RuinsUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints RuinsUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = lerp(p.fog, p.leaf, 0.55f) * 0.55f;
    h.light = false;
    h.ink = p.ink;
    h.accent = p.accent;
    h.styles = TS_INK | TS_LUMA | TS_RIPPLE;
    return h;
}

// =====================================================================================================
// Events
// =====================================================================================================

void RuinsUniverse::releaseBirds(int n, int side) {
    // from the trees beside the well (one side, or both), up and away
    for (int i = 0; i < n && birds_.size() < 40; i++) {
        const float s = side != 0 ? (float)side : (i % 2 ? 1.f : -1.f);
        Bird b;
        b.p = V(s * rng_.range(11.5f, 17.f), rng_.range(1.f, 7.f));
        b.v = V(s * rng_.range(1.0f, 3.2f), rng_.range(2.2f, 4.0f));
        b.age = -rng_.range(0.f, 0.5f);
        b.phase = rng_.range(0.f, TAU);
        b.size = rng_.range(0.22f, 0.36f);
        b.glide = rng_.uniform();
        birds_.push_back(b);
    }
}

void RuinsUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::HardDrop: {
        // a little dust pushed out from under the piece
        float yLo = 99.f, xs = 0.f;
        int n = 0;
        for (auto& c : ev.cells) yLo = std::min(yLo, layout::cellPos((float)c.x, (float)c.y).y);
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > yLo + 0.01f) continue;
            xs += p.x;
            n++;
        }
        if (!n) break;
        xs /= n;
        for (int i = 0; i < 6; i++) {
            Bit b;
            b.p = V(xs + rng_.range(-0.5f, 0.5f) * n * 0.5f, yLo - 0.5f);
            b.v = V(rng_.range(-1.f, 1.f), rng_.range(0.05f, 0.3f));
            b.age = 0.f;
            b.life = rng_.range(0.5f, 0.9f);
            b.size = rng_.range(0.16f, 0.26f);
            b.spin = 0.f;
            b.seed = rng_.uniform();
            b.col = lerp(pal_.stone, pal_.ink, 0.3f);
            b.mode = 0;
            b.alpha = 0.25f;
            puffs_.push_back(b);
        }
        break;
    }
    case GameEvent::Clear: {
        int nCells = 0;
        for (auto& cl : ev.cells) {
            if (cl.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)cl.x, (float)cl.y);
            const int type = std::clamp(cl.type, 0, 6);
            crumbling_.push_back({p.x, p.y, type, 0.f, rng_.uniform()});
            nCells++;
            // chips falling away behind the well, smaller as they recede
            for (int k = 0; k < 3; k++) {
                Bit b;
                b.p = V(p.x + rng_.range(-0.35f, 0.35f), p.y + rng_.range(-0.3f, 0.3f));
                b.v = V(rng_.range(-1.6f, 1.6f), rng_.range(0.2f, 2.4f));
                b.age = -rng_.range(0.05f, 0.3f);
                b.life = rng_.range(0.9f, 1.4f);
                b.size = rng_.range(0.07f, 0.16f);
                b.spin = rng_.range(-6.f, 6.f);
                b.seed = rng_.uniform();
                b.col = pal_.piece[type] * rng_.range(0.6f, 0.9f);
                b.mode = 2;
                b.alpha = 1.f;
                chips_.push_back(b);
            }
            if (nCells % 2 == 0) {
                Bit b;
                b.p = V(p.x, p.y - 0.2f);
                b.v = V(rng_.range(-0.4f, 0.4f), rng_.range(-0.4f, 0.1f));
                b.age = 0.f;
                b.life = rng_.range(1.0f, 1.5f);
                b.size = rng_.range(0.7f, 1.0f);
                b.spin = 0.f;
                b.seed = rng_.uniform();
                b.col = lerp(pal_.stone, pal_.piece[type], 0.4f);
                b.mode = 0;
                b.alpha = 0.3f;
                dust_.push_back(b);
            }
        }
        // the noise startles a few birds; a quad a whole flock
        if (ev.count >= 4) releaseBirds(12, 0);
        else if (rng_.chance(0.4f)) releaseBirds(3 + rng_.irange(0, 3 - 1), rng_.chance(0.5f) ? 1 : -1);
        // each clear sends dust to the next repairs
        if (active_ >= 0) dustAt(focusP_, 16);
        break;
    }
    case GameEvent::TopOut: {
        overT_ = 0.f;
        overCells_.clear();
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.x < 0 || c.x >= Game::W || c.y >= Game::H) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            overCells_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), (float)c.y});
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Pieces
// =====================================================================================================

void RuinsUniverse::cell(float x, float y, float half, int type, float alpha, int mode, float erase, float seed) {
    type = std::clamp(type, 0, 6);
    const vec3& tint = pal_.piece[type];
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, seed}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, 0.f, (float)type}});
}

void RuinsUniverse::buildBoard(const Game& g) {
    auto seedOf = [](int x, int y) { return (float)((x * 7 + y * 13) % 17) / 17.f; };
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const float off = g.rowOffset(y);
            const vec3 p = layout::cellPos((float)x, y - off);
            cell(p.x, p.y, 0.5f, c.type, 1.f, 0, 0.f, seedOf(x, y));
        }
    // game over: the blocks grey and darken from the top down, like stones long fallen
    if (overT_ >= 0.f)
        for (const Over& o : overCells_) {
            const float rowFrac = (o.row - Game::HIDDEN) / (float)(Game::H - Game::HIDDEN);
            const float k = smoothstepf(rowFrac * 0.6f, rowFrac * 0.6f + 0.3f, over_);
            cell(o.x, o.y, 0.5f, o.type, 1.f - 0.4f * smoothstepf(0.7f, 1.f, over_), 0, 0.f, 0.f);
            cells_.back().c[2] = k;
        }
    for (const Crumble& c : crumbling_) cell(c.x, c.y, 0.5f, c.type, 1.f, 0, std::max(0.001f, saturate(c.t / CLEAR_T)), c.seed);
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
                cell(w.x, w.y, 0.5f, p.type, 0.6f, 1, 0.f, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, p.type, 1.f, 0, 0.f, seedOf(c[0], c[1]));
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
        for (auto& c : cells) {
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            cell(px, py, 0.5f * sc, type, alpha, 0, 0.f, seedOf(c[0], c[1]));
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void RuinsUniverse::sprite(std::vector<SpriteInst>& to, vec2 p, float size, const vec3& col, float alpha, int mode, float param,
                           float param2) {
    if (alpha <= 0.004f || size <= 0.f) return;
    to.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {param, param2, 0.f, 0.f}});
}

void RuinsUniverse::drawSprites(const std::vector<SpriteInst>& list) {
    if (list.empty()) return;
    glUseProgram(progSprite_);
    setMat(progSprite_, "uVP", vp_);
    set1f(progSprite_, "uPxPerUnit", pxPerUnit_);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    glBufferData(GL_ARRAY_BUFFER, list.size() * sizeof(SpriteInst), list.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)list.size());
}

// =====================================================================================================
// The HUD
// =====================================================================================================

void RuinsUniverse::drawHud(const std::vector<HudRect>& panels) {
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

// The lettering, by the host (text.hpp): Marcellus' carved capitals, widely spaced (like an inscription); the song,
// the variant and the notices in Cormorant Garamond's italics, the universe's line ending with the hour of the light;
// each line on a halo of the veil. The inks are the scene's (linear), toned as the post tones the scene: dimmed,
// their highlights rolled off, to sRGB.
void RuinsUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* caps = text::font(FONT_CAPS);
    text::Font* ital = text::font(FONT_ITAL);
    const float s = height_ / 1080.f;
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount), dim = 1.f - dimT_ * 0.5f;
    auto tone = [&](const vec3& lin) {
        vec3 c = lin * dim;
        for (float* v : {&c.x, &c.y, &c.z}) *v /= 1.f + std::max(*v - 0.7f, 0.f) * 0.85f;
        return ugl::toSrgb(c);
    };
    auto inkOf = [&](const vec3& c) { return tone(lerp(c, bridge_.ink, b)); };
    const vec3 ink = inkOf(pal_.ink), accent = inkOf(pal_.accent);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    float halo = 0.9f; // the halo's amount
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        // the name in spaced capitals, then the variant and the hour of the light in italics, higher
        const char* hour = slow_ > 0.5f ? "blue hour"
                           : gold_ > 0.5f ? "golden hour"
                           : linesEased_ < 12.f ? "overgrown"
                           : prime_ > 0.5f ? "in its prime"
                                           : "time rewinding";
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        size_t n = 1;
        while (n < var.size() && ((unsigned char)var[n] & 0xC0) == 0x80) n++;
        if (!var.empty()) var = var.substr(0, n) + text::lower(var.substr(n));
        const float sz = base * 0.95f, tr = sz * 0.3f, vsz = base * 1.35f, gap = base * 0.7f;
        const float a = saturate(0.35f + h.alpha * 1.6f);
        // (each run was placed by its own cap line; a span's spacing stays after its last letter, the gaps take it
        // back)
        const float dy = -sz * 0.3f + text::capHeight(ital) * vsz - text::capHeight(caps) * sz;
        put(caps, name, sz, tr, ink, a);
        if (!var.empty()) {
            text::Span& v = out.add(var, ital, vsz, accent, 0.f, a * 0.92f);
            v.gap = (gap - tr) / vsz;
            v.dy = dy;
        }
        text::Span& w = out.add(hour, ital, vsz, ink, 0.f, a * 0.7f);
        w.gap = (gap * 1.4f - (var.empty() ? tr : 0.f)) / vsz;
        w.dy = dy;
        break;
    }
    case HUD_SECTION: put(caps, h.text, base * 0.9f, base * 0.5f, ink, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(ital, h.text, base * 1.8f, base * 0.01f, ink, h.alpha * 1.3f, -base * 0.4f); break;
    case HUD_INFO: put(caps, h.text, base * 0.66f, base * 0.16f, ink, saturate(alpha * 1.3f) * 0.8f); break;
    case HUD_LABEL: put(caps, h.text, base * 0.82f, base * 0.45f, inkOf(lerp(pal_.ink, pal_.accent, 0.55f)), saturate(alpha * 1.2f) * 0.95f); break;
    case HUD_VALUE: put(caps, h.text, base * 1.08f, base * 0.08f, ink, alpha); break;
    case HUD_NOTICE: put(ital, h.text, base * 1.7f, base * 0.01f, accent, alpha); break;
    case HUD_KEY: put(caps, h.text, base * 0.75f, base * 0.1f, ink, alpha); break;
    case HUD_TOAST: put(caps, h.text, base * 1.3f, base * 0.5f, ink, alpha, -base * 0.2f); halo = 0.85f; break;
    case HUD_TOAST_SUB: put(ital, h.text, base * 1.5f, base * 0.01f, accent, alpha * 0.9f); halo = 0.85f; break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(caps, h.text, base * 1.15f, base * 0.3f, ink, alpha, -base * 0.1f);
        else put(caps, h.text, base * 0.85f, base * 0.16f, h.accent ? accent : ink, alpha);
        halo = 0.85f;
        break;
    default: put(caps, h.text, base * 0.85f, base * 0.14f, h.accent ? accent : ink, alpha); break;
    }
    // the letters as the scene drew them, on a halo of the veil
    text::Lettering& l = lettering_[halo < 0.88f ? 1 : 0];
    l.glsl = shaders::LETTER_GLSL;
    l.p0 = vec4(tone(lerp(pal_.veil * 0.7f, bridge_.color, b)), halo);
    out.lettering = &l;
    out.alpha = saturate(fade_);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void RuinsUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    // REFALL_GPUTIME: the GPU time of a frame, printed every 60 frames
    static const bool gpuTime = std::getenv("REFALL_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 3.f) : 0.f, overT_ >= 0.f ? 10.f : 0.8f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
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
    const bool isDrop = overT_ < 0.f && (sectionKnown ? sectionDrop : dropMode_ && (!music.hasSong || music.speed > 1.26f));
    // the golden hour: in over a few seconds when the drop lands, held for the whole drop, out slowly after it
    gold_ = approach(gold_, isDrop ? 1.f : 0.f, isDrop ? 0.9f : 0.3f, dt);
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { pal_ = target; palSet_ = true; }
        else lerpPalette(pal_, target, 1.f - std::exp(-dt * 1.0f));
    }
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
    // the leaves sway more as the music rises (slow envelopes; the phase runs a little faster too)
    swayAmp_ = approach(swayAmp_, 0.35f + 0.4f * wm + 0.35f * wp + 0.2f * gold_, 0.5f, dt);
    sway_ += wdt * (0.6f + 0.5f * wm + 0.4f * wp);
    // the drift
    if (!f.paused) camPhase_ += wdt * (1.f + 0.25f * wm + 0.2f * wp);
    updateCamera();

    // ---- Birds: on strong beats of a peak (every two beats at most, a cooldown between flocks), now and then
    // in calmer parts.
    birdCool_ -= wdt;
    {
        const double half = std::floor(beat / 2.0);
        if (half != lastHalf_) {
            lastHalf_ = half;
            if (wp > 0.5f && birdCool_ <= 0.f && std::max(music.kick, kickIn_) > 0.5f) {
                releaseBirds(4 + rng_.irange(0, 4 - 1), rng_.chance(0.5f) ? 1 : -1);
                birdCool_ = 6.f;
            }
        }
        calmBirds_ -= wdt;
        if (calmBirds_ <= 0.f) {
            releaseBirds(2 + rng_.irange(0, 2 - 1), rng_.chance(0.5f) ? 1 : -1);
            calmBirds_ = rng_.range(14.f, 26.f);
        }
    }
    for (Bird& b : birds_) {
        b.age += wdt;
        if (b.age < 0.f) continue;
        b.p = V(b.p.x + b.v.x * wdt, b.p.y + b.v.y * wdt);
        b.v = V(b.v.x * (1.f + 0.15f * wdt), b.v.y * std::exp(-0.25f * wdt));
        const float glide = b.age > 1.2f && std::sin(b.age * 1.3f + b.glide * 6.f) > 0.6f ? 0.15f : 1.f;
        b.phase += wdt * 15.f * glide;
    }
    birds_.erase(std::remove_if(birds_.begin(), birds_.end(),
                                [&](const Bird& b) { return b.age > 7.f || std::fabs(b.p.x) > viewH_.x + 1.5f || b.p.y > viewH_.y + 1.5f; }),
                 birds_.end());
    for (Fly& fl : flies_) {
        const float t = time_ * (0.5f + 0.25f * wm + 0.25f * wp);
        fl.p = V(fl.home.x + 1.6f * std::sin(t * 0.37f + fl.phase) + 0.6f * std::sin(t * 1.1f + fl.seed * 9.f),
                 fl.home.y + 1.1f * std::sin(t * 0.29f + fl.phase * 1.7f) + 0.35f * std::sin(t * 1.7f + fl.seed * 4.f));
        fl.phase += 0.f;
    }

    // ---- The rewind: the lines (50-line scale) eased (a line a breath, a quad a few; a new game sends time
    // forward again, quickly).
    {
        const float target = f.bare ? lines_ : paceLines(game, (float)extraLines(time_));
        if (linesEased_ < 0.f) linesEased_ = target;
        lines_ = target;
        const float cap = (target < linesEased_ ? 10.f : 0.9f * (1.f + 0.5f * gold_)) * std::clamp(paceScale(), 1.f, 2.5f);
        const bool reset = linesEased_ >= 0.f && target < linesEased_ - 6.f; // a new game: time forward again
        const bool first = planFor_ < 0;
        linesEased_ += std::clamp((target - linesEased_) * 1.4f * wdt, -cap * wdt, cap * wdt);
        const int pf = variant_ * 2 + (side_ > 0.f ? 1 : 0);
        if (pf != planFor_ || reset) {
            buildPlan();
            planFor_ = pf;
            next_ = 0;
            active_ = -1;
            for (float& v : colRep_) v = 0.f;
            templeRep_ = 0.f;
            if (reset) linesEased_ = target;
        }
        const int owed = owedEvents(target);
        if (first || reset || f.bare) {
            if (first || reset) snapNext_ = true;
            // (coming in mid-way, or a test still: what is owed is already done)
            while (next_ < owed) applyEv(plan_[next_++], 1.f);
        }
        // the next repair: the camera goes to it first; it starts once it is framed, lasts ~2.5 s (shorter when
        // they pile up), glowing, dust converging on it
        if (active_ < 0 && next_ < owed && !f.paused) {
            active_ = next_++;
            evT_ = -1.f;
            travelT_ = 0.f;
        }
        focusA_ = approach(focusA_, 0.f, 2.f, dt);
        if (active_ >= 0) {
            const Ev& e = plan_[active_];
            focusPose(e, goalEye_, goalTgt_);
            focusP_ = e.focus;
            focusR_ = e.kind == 2 ? 6.f : 2.6f;
            if (evT_ < 0.f) {
                travelT_ += wdt;
                const bool framed = length(camEye_ - goalEye_) < 3.5f && length(camTgt_ - goalTgt_) < 5.f;
                if (framed || travelT_ > 5.f) evT_ = 0.f;
            } else {
                const float dur = (e.kind == 2 ? 2.6f : 3.2f) * ((owed - next_) > 1 ? 0.7f : 1.f);
                evT_ += wdt;
                const float u = saturate(evT_ / dur);
                applyEv(e, smoothstepf(0.f, 1.f, u));
                focusA_ = std::max(focusA_, std::sin(PI * std::min(1.f, u * 1.15f)));
                dustT_ -= wdt;
                if (dustT_ <= 0.f) {
                    const float r = focusR_ * 0.5f;
                    dustAt(e.focus + vec3(rng_.range(-r, r), rng_.range(-r, r), rng_.range(-r * 0.3f, r * 0.3f)), 4);
                    dustT_ = 0.08f;
                }
                if (u >= 1.f) {
                    active_ = -1;
                    idleT_ = 0.f;
                }
            }
        } else {
            idleT_ += wdt;
            // between the repairs the camera stays on the last one a moment, then pulls back to the whole temple
            if (idleT_ > 3.f || next_ == 0 || next_ >= (int)plan_.size()) {
                goalEye_ = ovEye_;
                goalTgt_ = ovTgt_;
            }
        }
        updateRewind(linesEased_);
        if (!camInit_) {
            camEye_ = goalEye_;
            camTgt_ = goalTgt_;
            camInit_ = true;
        }
        stepCamera(f.paused ? 0.f : wdt);
        // the setup: the camera comes in from a step back; the teardown: it pulls back
        const float away = 1.f - smoothstepf(0.f, 0.8f, f.setup) + smoothstepf(0.05f, 1.f, f.teardown);
        if (away > 0.f) {
            eye_ = camEye_ - fwd_ * (14.f * away) + vec3(0.f, 1.5f * away, 0.f);
            fwd_ = normalize(camTgt_ - eye_);
            right_ = normalize(cross(fwd_, vec3(0.f, 1.f, 0.f)));
            up_ = cross(right_, fwd_);
        }
        for (Swirl& s : swirls_) s.age += dt;
        swirls_.erase(std::remove_if(swirls_.begin(), swirls_.end(), [](const Swirl& s) { return s.age >= s.life; }), swirls_.end());
    }

    // ---- Dust, chips, crumbling blocks (at the real speed).
    auto stepBits = [&](std::vector<Bit>& v, float grav, float drag) {
        for (Bit& b : v) {
            b.age += dt;
            if (b.age < 0.f) continue;
            b.v = V(b.v.x * std::exp(-drag * dt), (b.v.y - grav * dt) * std::exp(-drag * dt));
            b.p = V(b.p.x + b.v.x * dt, b.p.y + b.v.y * dt);
            b.spin += 0.f;
        }
        v.erase(std::remove_if(v.begin(), v.end(), [](const Bit& b) { return b.age >= b.life; }), v.end());
    };
    stepBits(chips_, 13.f, 0.3f);
    stepBits(dust_, -0.2f, 1.2f);
    stepBits(puffs_, 0.f, 2.5f);
    for (Crumble& c : crumbling_) c.t += dt;
    crumbling_.erase(std::remove_if(crumbling_.begin(), crumbling_.end(), [](const Crumble& c) { return c.t > CLEAR_T; }), crumbling_.end());

    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);

    // ---- The light: late morning, high sun behind the temple (the shafts toward us); the drop's golden hour, a
    // low warm sun on the left raking across the stones; slow motion a cool blue hush.
    const int kind = VARIANTS[wrapV(variant_)].kind;
    const float g = smoothstepf(0.f, 1.f, gold_);
    vec3 sunDir = normalize(lerp(normalize(vec3(-0.5f, 0.76f, 0.3f)), normalize(vec3(-0.82f, 0.2f, 0.36f)), g));
    vec3 sunC = lerp(pal_.sun * 2.5f, vec3(3.8f, 1.85f, 0.6f), g);
    vec3 skyZ = lerp(pal_.skyZ, vec3(0.42f, 0.42f, 0.52f), g);
    vec3 skyH = lerp(pal_.skyH, vec3(1.25f, 0.78f, 0.34f), g);
    vec3 amb = lerp(pal_.amb * 0.9f, pal_.amb * vec3(0.95f, 0.66f, 0.5f) * 0.7f, g);
    vec3 fog = lerp(pal_.fog, vec3(1.0f, 0.7f, 0.36f), g);
    float fogK = VARIANTS[wrapV(variant_)].fogK * (1.f + 0.15f * g) * (1.f + 0.25f * (1.f - wm));
    // the setup: the morning mist lifts off the ruins; the teardown: it settles back as the sun weakens
    const float haze = 1.f - smoothstepf(0.1f, 0.9f, f.setup) + smoothstepf(0.15f, 1.f, f.teardown);
    fogK *= 1.f + 0.8f * haze;
    float mist = lerpf(lerpf(1.4f, 0.9f, wm), 0.6f, wp) * (1.f + 0.1f * g) * (1.f + 1.2f * haze);
    float shaft = (1.f + 0.25f * wp) * (1.f + 1.1f * g);
    sunC = sunC * (1.2f * (1.f - 0.5f * smoothstepf(0.15f, 1.f, f.teardown)));
    float gap = VARIANTS[wrapV(variant_)].gap + 0.16f * g;
    {
        const vec3 hush(0.55f, 0.7f, 1.15f);
        sunC = lerp(sunC, sunC * hush * 0.45f, slow_);
        amb = lerp(amb, amb * hush * 0.8f, slow_);
        skyH = lerp(skyH, skyH * hush * 0.7f, slow_);
        skyZ = lerp(skyZ, skyZ * hush * 0.7f, slow_);
        fog = lerp(fog, fog * hush * 0.7f, slow_);
        // the end of a game: the light fades a little
        sunC = sunC * (1.f - 0.4f * over_);
    }

    // ---- Sprites.
    back_.clear();
    front_.clear();
    const vec3 birdC = lerp(pal_.leaf * 0.25f, vec3(0.05f, 0.03f, 0.02f), g);
    if (!f.bare) {
        for (const Bit& b : chips_) {
            if (b.age < 0.f) continue;
            const float t = b.age / b.life;
            // receding as it falls: smaller, darker, fading into the haze
            const float sz = b.size * (1.f - 0.55f * t);
            sprite(back_, b.p, sz, b.col * (1.f - 0.5f * t), 1.f - smoothstepf(0.6f, 1.f, t), 2, b.spin * b.age + b.seed * 6.f, b.seed);
        }
        for (const Bit& b : dust_) {
            const float t = b.age / b.life;
            sprite(back_, b.p, b.size * (1.f + 0.8f * t), b.col, b.alpha * smoothstepf(0.f, 0.12f, t) * (1.f - smoothstepf(0.3f, 1.f, t)), 0, b.seed);
        }
        for (const Bit& b : puffs_) {
            const float t = b.age / b.life;
            sprite(front_, b.p, b.size * (1.f + t), b.col, b.alpha * smoothstepf(0.f, 0.1f, t) * (1.f - smoothstepf(0.5f, 1.f, t)), 0, b.seed);
        }
    }
    {
        const vec3 dc = lerp(vec3(1.0f, 0.82f, 0.55f), vec3(1.4f, 0.9f, 0.45f), g);
        for (const Swirl& s : swirls_) {
            if (s.age < 0.f) continue;
            const float t = s.age / s.life, e = smoothstepf(0.f, 1.f, t);
            const float a = s.ang + e * 4.f;
            const vec2 p = V(s.c.x + std::cos(a) * s.rad * (1.f - e), s.c.y + std::sin(a) * s.rad * 0.7f * (1.f - e));
            sprite(back_, p, s.size * (1.f - 0.5f * e), dc, 0.8f * smoothstepf(0.f, 0.15f, t) * (1.f - smoothstepf(0.8f, 1.f, t)) * world, 0, s.seed);
        }
    }
    for (const Bird& b : birds_) {
        if (b.age < 0.f) continue;
        const float a = smoothstepf(0.f, 0.3f, b.age) * (1.f - smoothstepf(5.5f, 7.f, b.age)) * 0.9f * world;
        sprite(back_, b.p, b.size * (1.f - 0.25f * saturate(b.age / 6.f)), birdC, a, 3, b.phase);
    }
    {
        static const unsigned FLY_C[4] = {0x5aa8ff, 0xffd060, 0xff8a40, 0xf4f0e0};
        for (const Fly& fl : flies_) {
            const vec3 c = ugl::toLinear(hex(FLY_C[(int)fl.hue & 3])) * lerpf(0.8f, 1.1f, g);
            sprite(back_, fl.p, 0.2f, c, 0.85f * world * (1.f - over_), 4, time_ * 9.f + fl.phase);
        }
    }
    cells_.clear();
    if (!f.bare) buildBoard(game);

    // =========================================================== the world (meshes, multisampled)
    if (scene_.kind() != kind) buildScene(kind);
    {
        ruins::State st;
        st.kind = kind;
        st.jungle = jungle_;
        st.temple = temple_;
        st.broken = broken_;
        st.wear = wear_;
        st.prime = prime_;
        for (int i = 0; i < 8; i++) st.col[i] = col_[i];
        scene_.animate(st, f.paused ? 0.f : wdt, snapNext_ || f.bare, inst_, draws_);
        snapNext_ = false;
        glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
        glBufferData(GL_ARRAY_BUFFER, inst_.size() * sizeof(ruins::Inst), inst_.data(), GL_STREAM_DRAW);
        if (scene_.cornerChanged()) {
            const auto& ct = scene_.cornerTex();
            glBindTexture(GL_TEXTURE_2D, cornerTex_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ruins::Scene::CORNER_W, (int)ct.size() / ruins::Scene::CORNER_W, 0, GL_RED,
                         GL_UNSIGNED_BYTE, ct.data());
        }
        if (scene_.figChanged()) {
            glBindBuffer(GL_ARRAY_BUFFER, figVbo_);
            glBufferData(GL_ARRAY_BUFFER, scene_.figVerts().size() * sizeof(ruins::Vtx), scene_.figVerts().data(), GL_DYNAMIC_DRAW);
        }
    }
    vp3_ = mat4::perspective(2.f * std::atan(tanH_), aspect_, NEAR, FAR) * mat4::lookAt(eye_, eye_ + fwd_, up_);
    {
        // the sun's shadow map, fitted to the scene as the sun sees it
        const vec3 C(0.f, 10.f, -45.f);
        const mat4 lv = mat4::lookAt(C + sunDir * 200.f, C, vec3(0.f, 1.f, 0.f));
        float mn[3] = {1e9f, 1e9f, 1e9f}, mx[3] = {-1e9f, -1e9f, -1e9f};
        for (int i = 0; i < 8; i++) {
            const vec4 l = lv * vec4((i & 1) ? 54.f : -54.f, (i & 2) ? 66.f : -1.f, (i & 4) ? 24.f : -120.f, 1.f);
            const float c[3] = {l.x, l.y, l.z};
            for (int k = 0; k < 3; k++) { mn[k] = std::min(mn[k], c[k]); mx[k] = std::max(mx[k], c[k]); }
        }
        lightVp_ = mat4::ortho(mn[0], mx[0], mn[1], mx[1], -mx[2] - 5.f, -mn[2] + 5.f) * lv;
    }
    auto setScene = [&](GLuint P) {
        set3f(P, "uEye", eye_);
        set3f(P, "uFwd", fwd_);
        set3f(P, "uRight", right_);
        set3f(P, "uUp", up_);
        set1f(P, "uTanH", tanH_);
        set1f(P, "uAspect", aspect_);
        set1f(P, "uTime", std::fmod(sway_, 2000.f));
        set1f(P, "uPx", 2.f * tanH_ / msH_);
        set1i(P, "uKind", kind);
        set3f(P, "uSunDir", sunDir);
        set3f(P, "uSunCol", sunC);
        set3f(P, "uSkyZ", skyZ);
        set3f(P, "uSkyH", skyH);
        set3f(P, "uAmb", amb);
        set3f(P, "uFog", fog);
        set3f(P, "uStone", pal_.stone);
        set3f(P, "uStone2", pal_.stone2);
        set3f(P, "uMoss", pal_.moss);
        set3f(P, "uMoss2", pal_.moss2);
        set3f(P, "uLeaf", pal_.leaf);
        set3f(P, "uLeafLit", pal_.leafLit);
        set3f(P, "uBark", pal_.bark);
        set3f(P, "uGround", pal_.ground);
        set1f(P, "uFogK", fogK);
        set1f(P, "uMist", mist);
        set1f(P, "uShaft", shaft);
        set1f(P, "uGap", gap);
        set1f(P, "uSway", swayAmp_);
        set1f(P, "uGold", g);
        set1f(P, "uWorldA", world);
        set1f(P, "uJungle", jungle_);
        set1f(P, "uTemple", temple_);
        set1f(P, "uBroken", broken_);
        set1f(P, "uWear", wear_);
        set1f(P, "uPrime", prime_);
        set4f(P, "uFocus", focusP_.x, focusP_.y, focusP_.z, focusA_);
        set1f(P, "uFocusR", focusR_);
        set3f(P, "uBridgeCol", bridge_.color);
        setMat(P, "uLightVP", lightVp_);
        set1i(P, "uShadow", 3);
        set1i(P, "uCornerTex", 4);
        set1f(P, "uSoft", 1.7f / SHADOW_SIZE);
        set2f(P, "uNF", NEAR, FAR);
    };
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    // ---- the sun's shadow map
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 2.f);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, cornerTex_);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(progShadow_);
    setScene(progShadow_); // (MAYA's corner: its stones' dissolve, its bowl's clip)
    setMat(progShadow_, "uVP", lightVp_);
    drawMeshes(progShadow_);
    glUseProgram(progBushShadow_);
    setScene(progBushShadow_);
    setMat(progBushShadow_, "uVP", lightVp_);
    set1f(progBushShadow_, "uLit", 0.f);
    drawBushes(bushShells_, bushCores_);
    glDisable(GL_POLYGON_OFFSET_FILL);
    // ---- the sky, the meshes, the foliage: multisampled, at the full resolution
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, msW_, msH_);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(progSky_);
    setScene(progSky_);
    ugl::drawFullscreen();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glUseProgram(progMesh_);
    setScene(progMesh_);
    setMat(progMesh_, "uVP", vp3_);
    drawMeshes(progMesh_);
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glUseProgram(progFol_);
    setScene(progFol_);
    setMat(progFol_, "uVP", vp3_);
    glBindVertexArray(folVao_);
    glDrawArrays(GL_TRIANGLES, 0, folCount_);
    glUseProgram(progBush_);
    setScene(progBush_);
    setMat(progBush_, "uVP", vp3_);
    set1f(progBush_, "uLit", 1.f);
    drawBushes(bushShells_, bushCores_ + canCores_); // (the cores first: the leaves behind them fail the depth test)
    drawBushes(0, bushShells_);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, worldT_.fbo);
    glBlitFramebuffer(0, 0, msW_, msH_, 0, 0, msW_, msH_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, 0, msW_, msH_, 0, 0, msW_, msH_, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    // ---- the mist, the light shafts, the occlusion (reduced resolution)
    volT_.bind();
    glUseProgram(progVol_);
    setScene(progVol_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldDepth_);
    set1i(progVol_, "uDepth", 0);
    set2f(progVol_, "uFull", (float)msW_, (float)msH_);
    ugl::drawFullscreen();

    // =========================================================== the scene (full resolution)
    sceneT_.bind();
    glUseProgram(progComp_);
    setScene(progComp_);
    set1f(progComp_, "uPx", 2.f * tanH_ / height_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progComp_, "uColor", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, worldDepth_);
    set1i(progComp_, "uDepth", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, volT_.tex);
    set1i(progComp_, "uVol", 2);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, auxTex_);
    set1i(progComp_, "uAux", 4);
    glActiveTexture(GL_TEXTURE0);
    set2f(progComp_, "uLow", (float)volT_.w, (float)volT_.h);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // behind the well: the chips and dust falling away, the birds, the butterflies
    drawSprites(back_);
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", lerp(pal_.veil, bridge_.color, bridge));
        set3f(progVeil_, "uRail", lerp(lerp(pal_.stone, pal_.accent, 0.3f) * 0.45f, bridge_.ink * 0.4f, bridge));
        set1f(progVeil_, "uWellA", 0.8f);
        set1f(progVeil_, "uColA", 0.42f * world);
        set1f(progVeil_, "uRailA", 0.5f);
        set1f(progVeil_, "uPx", pxPerUnit_);
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], -5.f, layout::LEFT_COLUMN[3]);
        set4f(progVeil_, "uColR", 5.f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();
    }
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set3f(progCell_, "uLightC", vec3(1.f, 0.96f, 0.88f));
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    drawSprites(front_);
    drawHud(f.panels);
    fade_ = f.fade;
    glDisable(GL_BLEND);
    if (gpuQ) {
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(gpuQ, GL_QUERY_RESULT, &ns);
        static double acc = 0.0;
        static int n = 0;
        acc += ns * 1e-6;
        if (++n == 60) {
            std::fprintf(stderr, "[ruins] gpu %.2f ms/frame (%dx%d world x%d)\n", acc / n, msW_, msH_, samples_);
            acc = 0.0;
            n = 0;
        }
    }

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

// The world's meshes for a variant (world.cpp), uploaded, and its foliage cards (planes whose leafy masks the
// shader computes where they are: the far jungle's walls, the lianas) and its bushes and canopy (instanced blobs)
void RuinsUniverse::buildScene(int kind) {
    const auto t0 = std::chrono::steady_clock::now();
    scene_.build(kind);
    const double buildMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    glBindBuffer(GL_ARRAY_BUFFER, meshVbo_);
    glBufferData(GL_ARRAY_BUFFER, scene_.verts().size() * sizeof(ruins::Vtx), scene_.verts().data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, figVbo_);
    glBufferData(GL_ARRAY_BUFFER, scene_.figVerts().size() * sizeof(ruins::Vtx), scene_.figVerts().data(), GL_DYNAMIC_DRAW);
    std::vector<float> v;
    auto quad = [&](const vec3& a, const vec3& b, const vec3& c, const vec3& d, float layer) {
        for (const vec3* p : {&a, &b, &c, &a, &c, &d}) v.insert(v.end(), {p->x, p->y, p->z, layer});
    };
    auto qx = [&](float x, float z0, float z1, float y0, float y1, float L) {
        quad(vec3(x, y0, z0), vec3(x, y0, z1), vec3(x, y1, z1), vec3(x, y1, z0), L);
    };
    auto qz = [&](float z, float x0, float x1, float y0, float y1, float L) {
        quad(vec3(x0, y0, z), vec3(x1, y0, z), vec3(x1, y1, z), vec3(x0, y1, z), L);
    };
    qx(-42.f, -320.f, 120.f, -1.f, 70.f, 0.f);
    qx(42.f, -320.f, 120.f, -1.f, 70.f, 1.f);
    qz(-96.f, -280.f, 280.f, -1.f, 70.f, 2.f);
    qz(-140.f, -320.f, 320.f, -1.f, 80.f, 3.f);
    qz(-2.f, -40.f, 40.f, 0.f, 31.f, 13.f);
    qz(-18.f, -40.f, 40.f, 0.f, 31.f, 14.f);
    glBindBuffer(GL_ARRAY_BUFFER, folVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    folCount_ = (int)v.size() / 4;
    // the bushes: clumps of two or three blobs (a main one, smaller ones beside or over it), scattered in depth about
    // five rows across the court (rows 0..4, the clearing opening from the middle as the jungle recedes), along its
    // sides (5, 6), and while the jungle is thick before the temple (7) and in a tall thicket across the court (8)
    {
        uint32_t h = 0x9e3779b9u * (uint32_t)(kind + 1);
        auto rnd = [&]() {
            h ^= h << 13;
            h ^= h >> 17;
            h ^= h << 5;
            return (float)(h & 0xffffff) / 16777216.f;
        };
        std::vector<float> shells, cores;
        auto clump = [&](float x, float z, float hx, float hy, int row) {
            const float jit = (rnd() - 0.5f) * 2.5f, ax = std::fabs(x);
            const int n = rnd() < 0.5f ? 2 : 3;
            for (int k = 0; k < n; k++) {
                const bool main = k == 0;
                const float bhx = hx * (main ? 1.f : 0.5f + 0.3f * rnd()), bhy = hy * (main ? 1.f : 0.5f + 0.4f * rnd());
                const float bx = x + (main ? 0.f : (rnd() - 0.5f) * 2.3f * hx), bz = z + (main ? 0.f : (rnd() - 0.5f) * 1.5f * hx);
                const float cy = main ? 0.2f : rnd() * 0.7f - 0.2f, seed = rnd() * 64.f;
                const float b[12] = {bx, cy, bz, bhx, bhy, seed, (float)row, 0.f, ax, jit, 0.f, 0.f};
                shells.insert(shells.end(), b, b + 12);
                if (bhx > 0.55f) {
                    cores.insert(cores.end(), b, b + 12);
                    cores[cores.size() - 5] = 1.f;
                }
            }
        };
        for (int i = 0; i < 5; i++)
            for (float x = -41.f; x < 41.f; x += 1.5f + 0.8f * rnd()) {
                if (std::fabs(x) < (i == 0 ? 8.3f : 4.f)) continue;
                clump(x, 10.f - 11.f * i + (rnd() - 0.5f) * 2.6f, 0.85f + 0.5f * rnd(), (i == 0 ? 0.6f : 0.8f) + 0.6f * rnd(), i);
            }
        for (int s = 0; s < 2; s++)
            for (float z = -34.f; z < 6.f; z += 1.4f + 0.6f * rnd())
                clump((s ? 1.f : -1.f) * (11.f + (rnd() - 0.3f) * 1.6f), z, 0.9f + 0.5f * rnd(), 0.55f + 0.35f * rnd(), 5 + s);
        for (float x = -41.f; x < 41.f; x += 1.6f + 0.8f * rnd())
            if (std::fabs(x) > 3.5f) clump(x, -10.f + (rnd() - 0.5f) * 3.f, 1.1f + 0.5f * rnd(), 0.9f + 0.4f * rnd(), 7);
        for (float x = -60.f; x < 60.f; x += 3.2f + 1.6f * rnd())
            clump(x, -27.f + (rnd() - 0.5f) * 4.f, 2.6f + 1.2f * rnd(), 2.4f + 1.2f * rnd(), 8);
        // the canopy overhead (not the Mediterranean's): two bands of hanging leaf masses across the court (rows 9
        // and 10, the farther one open over the court), each a ragged lower edge of hanging clumps in depth under
        // tiers of broad flattened blobs; its lower edge in iC.w (darker deeper in), its cores cast no shadow
        std::vector<float> canCores;
        if (kind != 2) {
            const float ph[3] = {rnd() * 6.3f, rnd() * 6.3f, rnd() * 6.3f};
            auto edge = [&](float x, float base) {
                return base + 1.5f * std::sin(x * 0.11f + ph[0]) + 0.9f * std::sin(x * 0.27f + ph[1]) +
                       0.5f * std::sin(x * 0.61f + ph[2]);
            };
            auto blob = [&](float x, float y, float z, float hx, float hy, int row, float hang, float bot, bool core) {
                const float b[12] = {x, y, z, hx, hy, rnd() * 64.f, (float)row, 0.f, std::fabs(x), 0.f, hang, bot};
                shells.insert(shells.end(), b, b + 12);
                if (core) {
                    canCores.insert(canCores.end(), b, b + 12);
                    canCores[canCores.size() - 5] = 1.f;
                }
            };
            for (int band = 0; band < 2; band++) {
                const float zc = band == 0 ? -8.f : -30.f, x0 = band == 0 ? 0.f : 20.f;
                const int tiers = band == 0 ? 4 : 6;
                for (int s = 0; s < 2; s++)
                    for (float ax = x0 + rnd() * 1.5f; ax < 90.f; ax += 2.1f + 1.3f * rnd()) {
                        const float x = s ? ax : -ax;
                        if (band == 1 && ax < 20.f + 6.f * rnd()) continue;
                        // (over the well, band 0 hangs a little higher)
                        const float bot = edge(x + 40.f * band, band == 0 ? 15.6f : 24.f) +
                                          (band == 0 ? 3.6f * (1.f - smoothstepf(6.f, 17.f, ax)) : 0.f) + (rnd() - 0.5f) * 1.4f;
                        const int row = 9 + band;
                        if (rnd() < 0.65f) {
                            const float hx = 0.7f + 0.6f * rnd(), hy = 1.4f + 1.8f * rnd();
                            blob(x + (rnd() - 0.5f) * 1.2f, bot + hy * 0.4f, zc + (rnd() - 0.5f) * 9.f, hx, hy, row, 1.f, bot, false);
                        }
                        for (int t = 0; t < tiers; t++) {
                            const float hx = 2.3f + 1.4f * rnd() + 0.25f * t, hy = 1.2f + 0.7f * rnd();
                            blob(x + (rnd() - 0.5f) * 1.5f, bot + 2.0f + t * 2.5f + rnd() * 1.2f, zc + (rnd() - 0.5f) * 9.f - 0.8f * t,
                                 hx, hy, row, 0.f, bot, rnd() < 0.55f);
                        }
                    }
            }
        }
        // the far jungle's crowns (rows 11 and 12): domes of leaves along the tops of its walls (x = -42, 42; z = -96,
        // -140), over the line where FOL_FS's cards stop (crownLine())
        if (kind != 2)
            for (int L = 0; L < 4; L++) {
                const float base = L == 3 ? 36.f : 24.f, seed = L * 13.7f;
                const float u0 = L < 2 ? -320.f : (L == 2 ? -220.f : -280.f), u1 = L < 2 ? 120.f : -u0;
                for (float u = u0 + rnd() * 3.f; u < u1; u += 4.f + 3.f * rnd()) {
                    const float line = base - 1.f + 3.f * std::sin(u * 0.045f + seed * 1.7f) + 0.8f * std::sin(u * 0.13f + seed);
                    const float hx = 3.6f + 2.6f * rnd(), hy = 2.6f + 2.2f * rnd(), d = (rnd() - 0.5f) * 3.f;
                    const vec3 c = L < 2 ? vec3(L == 0 ? -42.f + d : 42.f + d, line + 0.6f + 2.4f * rnd(), u)
                                         : vec3(u, line + 0.6f + 2.4f * rnd(), (L == 2 ? -96.f : -140.f) + d);
                    const float row = L < 3 ? 11.f : 12.f;
                    const float b[12] = {c.x, c.y, c.z, hx, hy, rnd() * 64.f, row, 0.f, 0.f, 0.f, 0.f, 0.f};
                    shells.insert(shells.end(), b, b + 12);
                    if (rnd() < 0.4f) {
                        const float o = (rnd() - 0.5f) * hx, dx = L < 2 ? (L == 0 ? -1.f : 1.f) : o, dz = L < 2 ? o : -1.f;
                        const float b2[12] = {c.x + dx, c.y + hy * (0.6f + 0.4f * rnd()), c.z + dz, hx * 0.6f, hy * 0.6f,
                                              rnd() * 64.f, row, 0.f, 0.f, 0.f, 0.f, 0.f};
                        shells.insert(shells.end(), b2, b2 + 12);
                    }
                }
            }
        // the Mediterranean's umbrella pines (rows 11 and 12): flattened domes of needles over the trunks FOL_FS draws
        // on its walls, found as crowns() finds them (a cell of 1 / 0.11 along the wall, a pine where its hash > 0.45)
        if (kind == 2)
            for (int L = 0; L < 4; L++) {
                const float seed = L * 13.7f, H0 = (L == 3 ? 36.f : 24.f) - 10.f;
                const float u0 = L < 2 ? -320.f : (L == 2 ? -280.f : -320.f), u1 = L < 2 ? 120.f : -u0;
                for (float ci = std::floor(u0 * 0.11f + seed); ci < u1 * 0.11f + seed; ci += 1.f) {
                    const float ch = ghash(ci + seed * 7.f, (ci + seed * 7.f) * 0.371f + 17.13f);
                    if (ch <= 0.45f) continue;
                    const float w = (0.42f + 0.1f * ghash(ci + 3.f, (ci + 3.f) * 0.371f + 17.13f)) / 0.11f;
                    const float H = H0 + 8.f * (0.6f + 0.5f * ch) - 8.f * 0.35f;
                    const float u = (ci + 0.5f - seed + 0.06f * std::sin(H * 0.3f + ci)) / 0.11f;
                    const float row = L < 3 ? 11.f : 12.f;
                    for (int k = 0; k < 3; k++) {
                        const float o = k == 0 ? 0.f : (k == 1 ? -0.45f : 0.45f) * w, d = (rnd() - 0.5f) * (k == 0 ? 0.6f : 2.4f);
                        const float hx = w * (k == 0 ? 0.75f : 0.5f), hy = k == 0 ? 1.3f : 0.95f, y = H + 0.6f + (k == 0 ? 0.f : (rnd() - 0.7f) * 1.f);
                        const vec3 c = L < 2 ? vec3((L == 0 ? -42.f : 42.f) + d, y, u + o) : vec3(u + o, y, (L == 2 ? -96.f : -140.f) + d);
                        const float b[12] = {c.x, c.y, c.z, hx, hy, rnd() * 64.f, row, 0.f, 0.f, 0.f, 0.f, 0.f};
                        shells.insert(shells.end(), b, b + 12);
                    }
                }
            }
        bushShells_ = (int)shells.size() / 12;
        bushCores_ = (int)cores.size() / 12;
        canCores_ = (int)canCores.size() / 12;
        shells.insert(shells.end(), cores.begin(), cores.end());
        shells.insert(shells.end(), canCores.begin(), canCores.end());
        glBindBuffer(GL_ARRAY_BUFFER, bushInstVbo_);
        glBufferData(GL_ARRAY_BUFFER, shells.size() * sizeof(float), shells.data(), GL_STATIC_DRAW);
    }
    snapNext_ = true;
    if (std::getenv("REFALL_GPUTIME"))
        std::fprintf(stderr, "[ruins] %s: %d triangles in the mesh buffer, %d drawn (with the instances), built in %.1f ms\n",
                     VARIANTS[wrapV(variant_)].name, scene_.triangles(), scene_.instancedTriangles(), buildMs);
}

// The bushes' instances [first, first + count) (their shells, then their cores).
void RuinsUniverse::drawBushes(int first, int count) {
    if (count <= 0) return;
    glBindVertexArray(bushVao_);
    glBindBuffer(GL_ARRAY_BUFFER, bushInstVbo_);
    const size_t base = (size_t)first * 12 * sizeof(float);
    for (int i = 0; i < 3; i++)
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(base + sizeof(float) * 4 * i));
    glDrawElementsInstanced(GL_TRIANGLES, bushIdx_, GL_UNSIGNED_SHORT, nullptr, count);
    glBindVertexArray(0);
}

void RuinsUniverse::drawMeshes(GLuint) {
    for (const ruins::Draw& d : draws_) {
        if (d.dyn && scene_.figVerts().empty()) continue;
        glBindVertexArray(d.dyn ? figVao_ : meshVao_);
        glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
        const size_t base = (size_t)d.first * sizeof(ruins::Inst);
        for (int i = 0; i < 6; i++)
            glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(ruins::Inst), (void*)(base + sizeof(float) * 4 * i));
        glDrawArraysInstanced(GL_TRIANGLES, d.r.first, d.r.count, d.count);
    }
    glBindVertexArray(0);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_ruins() {
    static const UniverseEntry e{
        "RUINS", "time runs backwards over the song: the jungle recedes from overgrown ruins that repair themselves, fallen drums flying back onto their columns and stones re-forming from dust, until the temple stands in its prime; the blocks are stone carved with glyphs, and the drop turns the sun to golden hour",
        [] { return std::unique_ptr<Universe>(new RuinsUniverse()); },
        // Fits most songs; the calm, contemplative ones a little better.
        [](const SongLook& l) { return 0.8f + 0.25f * (1.f - l.density) + 0.1f * (1.f - l.dynamics); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
