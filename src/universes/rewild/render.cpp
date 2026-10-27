// REWILD's drawing (see rewild.cpp): the GL resources, the world's meshes (the land and its cut face, the plants), the
// camera, the frame's passes, the stack's glass cubes of seeds and the HUD.
//
// The frame: the sun's shadow map (the plants); then the world, multisampled, with depth: the sky, the land, the
// plants, the birds, the dust and the splashes, the rain; the well's pane over it (and the bridge); the seeds flying
// out and the glints of glass (not veiled, hidden by what stands in front of them); resolved, a copy of it (what the
// stack's glass shows); the ghost, the stack, the falling piece, the previews, the puffs of dust under a lock, the
// gauge, the HUD; the final pass.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/rewild/rewild.hpp"
#include "universes/rewild/shaders.hpp"
#include "universes/rewild/world.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

using namespace rewild_world;

namespace {

// Lora (Italic for titles, Medium for labels and numbers), SIL Open Font License, see OFL-Lora.txt.
const unsigned char FONT_ITALIC[] = {
#include "universes/rewild/Lora-Italic.ttf.inc"
};
const unsigned char FONT_ROMAN[] = {
#include "universes/rewild/Lora-Medium.ttf.inc"
};

namespace shaders = rewild_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

enum { W_WORLD = 1, W_SHADOW = 2 };
GLuint compile(const char* vs, const char* fs, const char* name, int with) {
    std::string f(fs), v(vs);
    for (std::string* s : {&f, &v}) {
        const size_t at = s->find('\n') + 1;
        std::string pre = shaders::COMMON_GLSL;
        if (with & W_WORLD) pre += shaders::WORLD_GLSL;
        if (with & W_SHADOW) pre += shaders::SHADOW_GLSL;
        s->insert(at, pre);
    }
    return ugl::program(v.c_str(), f.c_str(), name);
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
    if (std::fabs(det) < 1e-12f) return mat4::identity();
    for (int i = 0; i < 16; i++) r.m[i] = inv[i] / det;
    return r;
}

// Low-poly plant meshes: position, normal, anchor (a leaf clump's or a flower's center, a vine's top), material
// (0 bark, 1 leaves, 2 lighter leaves, 3 grass, 4 petals, 5 a flower's heart, 6 vines, 7 dead wood, 8 fronds), sway
// weight (0 at the foot .. 1 at the top).
struct PlantBuilder {
    std::vector<float>& v;
    float swayH = 1.f, swayK = 1.f;
    Rng r{11};
    float sway(const vec3& p) const { return swayK * std::pow(saturate(p.y / swayH), 1.5f); }
    void vert(const vec3& p, const vec3& n, const vec3& c, int m) {
        v.insert(v.end(), {p.x, p.y, p.z, n.x, n.y, n.z, c.x, c.y, c.z, (float)m, sway(p)});
    }
    void tri(const vec3& a, const vec3& b, const vec3& c, const vec3& ctr, int m) {
        vec3 n = cross(b - a, c - a);
        const float l = length(n);
        n = l > 1e-9f ? n / l : vec3(0, 1, 0);
        vert(a, n, ctr, m);
        vert(b, n, ctr, m);
        vert(c, n, ctr, m);
    }
    void quad(const vec3& a, const vec3& b, const vec3& c, const vec3& d, const vec3& ctr, int m) {
        tri(a, b, c, ctr, m);
        tri(a, c, d, ctr, m);
    }
    // a tapered cylinder from a to b
    void cyl(const vec3& a, const vec3& b, float r0, float r1, int sides, int m) {
        const vec3 ax = normalize(b - a);
        const vec3 u = normalize(cross(ax, std::fabs(ax.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0)));
        const vec3 w = cross(ax, u);
        for (int i = 0; i < sides; i++) {
            const float a0 = TAU * i / sides, a1 = TAU * (i + 1) / sides;
            const vec3 d0 = u * std::cos(a0) + w * std::sin(a0), d1 = u * std::cos(a1) + w * std::sin(a1);
            quad(a + d0 * r0, a + d1 * r0, b + d1 * r1, b + d0 * r1, a, m);
        }
    }
    // a lumpy clump of leaves: a subdivided icosahedron, displaced, flat-shaded
    void blob(const vec3& c, const vec3& rad, int level, int m) {
        const float t = (1.f + std::sqrt(5.f)) * 0.5f;
        std::vector<vec3> P = {vec3(-1, t, 0), vec3(1, t, 0), vec3(-1, -t, 0), vec3(1, -t, 0), vec3(0, -1, t), vec3(0, 1, t),
                               vec3(0, -1, -t), vec3(0, 1, -t), vec3(t, 0, -1), vec3(t, 0, 1), vec3(-t, 0, -1), vec3(-t, 0, 1)};
        for (vec3& p : P) p = normalize(p);
        std::vector<int> F = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
                              3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
        for (int l = 0; l < level; l++) {
            std::vector<int> G;
            for (size_t i = 0; i < F.size(); i += 3) {
                const int a = F[i], b = F[i + 1], cc = F[i + 2];
                const int ab = (int)P.size(); P.push_back(normalize(P[a] + P[b]));
                const int bc = (int)P.size(); P.push_back(normalize(P[b] + P[cc]));
                const int ca = (int)P.size(); P.push_back(normalize(P[cc] + P[a]));
                G.insert(G.end(), {a, ab, ca, b, bc, ab, cc, ca, bc, ab, bc, ca});
            }
            F.swap(G);
        }
        std::vector<vec3> Q(P.size());
        const float s0 = r.range(0.f, 10.f);
        for (size_t i = 0; i < P.size(); i++) {
            const vec3& p = P[i];
            const float k = 1.f + 0.16f * std::sin(p.x * 3.1f + s0) * std::sin(p.y * 2.7f + s0 * 1.3f) + 0.08f * std::sin(p.z * 5.3f + s0);
            Q[i] = c + vec3(p.x * rad.x, p.y * rad.y * (p.y < 0.f ? 0.75f : 1.f), p.z * rad.z) * k;
        }
        for (size_t i = 0; i < F.size(); i += 3) tri(Q[F[i]], Q[F[i + 1]], Q[F[i + 2]], c, m);
    }
    // a blade (grass, a frond) from `a` along `dir` (unit, horizontal part) arching: up, then out and down
    void blade(const vec3& a, const vec3& tip, float rise, float width, int m, const vec3& ctr, int segs = 3) {
        const vec3 side = normalize(cross(tip - a, vec3(0, 1, 0)));
        for (int i = 0; i < segs; i++) {
            const float t0 = (float)i / segs, t1 = (float)(i + 1) / segs;
            auto at = [&](float t) { return a + (tip - a) * t + vec3(0.f, rise * 4.f * t * (1.f - t), 0.f); };
            const float w0 = width * (1.f - t0 * 0.85f), w1 = width * (1.f - t1 * 0.85f);
            const vec3 p0 = at(t0), p1 = at(t1);
            quad(p0 - side * w0, p0 + side * w0, p1 + side * w1, p1 - side * w1, ctr, m);
        }
    }
    // a vine hanging from `top`, `len` down, waving a little: two crossed strips
    void vine(const vec3& top, float len, int m) {
        for (int k = 0; k < 2; k++) {
            const vec3 side = k == 0 ? vec3(0.06f, 0, 0) : vec3(0, 0, 0.06f);
            const int segs = 5;
            for (int i = 0; i < segs; i++) {
                const float t0 = (float)i / segs, t1 = (float)(i + 1) / segs;
                const vec3 p0 = top + vec3(0.12f * std::sin(t0 * 5.f), -len * t0, 0.1f * std::cos(t0 * 4.f));
                const vec3 p1 = top + vec3(0.12f * std::sin(t1 * 5.f), -len * t1, 0.1f * std::cos(t1 * 4.f));
                const float w0 = 1.f - 0.5f * t0, w1 = 1.f - 0.5f * t1;
                quad(p0 - side * w0, p0 + side * w0, p1 + side * w1, p1 - side * w1, top, m);
            }
        }
    }
};

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool RewildUniverse::init(int width, int height) {
    progSky_ = compile(ugl::FULLSCREEN_VS, shaders::SKY_FS, "rewild sky", W_WORLD);
    progGround_ = compile(shaders::GROUND_VS, shaders::GROUND_FS, "rewild ground", W_WORLD | W_SHADOW);
    progPlant_ = compile(shaders::PLANT_VS, shaders::PLANT_FS, "rewild plant", W_WORLD | W_SHADOW);
    progDepth_ = compile(shaders::PLANT_VS, shaders::DEPTH_FS, "rewild depth", 0);
    progBird_ = compile(shaders::BIRD_VS, shaders::BIRD_FS, "rewild bird", W_WORLD);
    progPart_ = compile(shaders::PART_VS, shaders::PART_FS, "rewild particle", W_WORLD);
    progRain_ = compile(ugl::FULLSCREEN_VS, shaders::RAIN_FS, "rewild rain", 0);
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "rewild veil", 0);
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "rewild cube", 0);
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "rewild gauge", 0);
    progText_ = compile(shaders::TEXT_VS, shaders::TEXT_FS, "rewild text", 0);
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "rewild shape", 0);
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "rewild post", 0);

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto instanced = [&](GLuint& vao, GLuint& vbo, int vec4s) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < vec4s; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)(sizeof(float) * 4 * vec4s), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    instanced(cellVao_, cellVbo_, 4);
    instanced(spriteVao_, spriteVbo_, 4);
    instanced(birdVao_, birdVbo_, 4);
    instanced(partVao_, partVbo_, 4);

    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));

    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    buildMeshes();

    // the sun's shadow map
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_RES, SHADOW_RES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
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
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) std::fprintf(stderr, "[gl] rewild shadow FBO incomplete\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // the life map
    lifeG_.assign(LIFE_RES * LIFE_RES, 0.f);
    lifeM_.assign(LIFE_RES * LIFE_RES, 0.f);
    lifeF_.assign(LIFE_RES * LIFE_RES, 0.f);
    lifeTex_.assign(LIFE_RES * LIFE_RES * 4, 0);
    glGenTextures(1, &lifeTexId_);
    glBindTexture(GL_TEXTURE_2D, lifeTexId_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, LIFE_RES, LIFE_RES, 0, GL_RGBA, GL_UNSIGNED_BYTE, lifeTex_.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    samples_ = std::clamp((int)maxSamples, 1, 4);

    buildFont(fontItalic_, FONT_ITALIC);
    buildFont(fontRoman_, FONT_ROMAN);
    pal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

// The world's meshes: the land (a grid denser near the well, wider far out, its cut face along the front) and the
// plants (each kind near and far: a simpler one for the distant forest).
void RewildUniverse::buildMeshes() {
    {
        std::vector<float> v;
        std::vector<unsigned> idx;
        const int C = 220, R = 200;
        for (int j = 0; j <= R; j++) {
            const float t = (float)j / R;
            const float z = FRONT_Z - 800.f * std::pow(t, 2.5f);
            const float hw = 34.f + 0.95f * (-z);
            for (int i = 0; i <= C; i++) {
                const float x = ((float)i / C * 2.f - 1.f) * hw;
                const vec3 n = groundN(x, z);
                v.insert(v.end(), {x, groundH(x, z), z, n.x, n.y, n.z});
            }
        }
        for (int j = 0; j < R; j++)
            for (int i = 0; i < C; i++) {
                const unsigned a = j * (C + 1) + i, b = a + 1, c = a + C + 1, d = c + 1;
                idx.insert(idx.end(), {a, c, b, b, c, d});
            }
        const unsigned base = (unsigned)(v.size() / 6);
        const int CF = 400;
        for (int i = 0; i <= CF; i++) {
            const float x = ((float)i / CF * 2.f - 1.f) * 70.f;
            v.insert(v.end(), {x, -40.f, FRONT_Z, 0.f, 0.f, 1.f});
            v.insert(v.end(), {x, groundH(x, FRONT_Z), FRONT_Z, 0.f, 0.f, 1.f});
        }
        for (int i = 0; i < CF; i++) {
            const unsigned a = base + i * 2, b = a + 1, c = a + 2, d = a + 3;
            idx.insert(idx.end(), {a, c, b, b, c, d});
        }
        glGenVertexArrays(1, &ground_.vao);
        glGenBuffers(1, &ground_.vbo);
        glGenBuffers(1, &ground_.ibo);
        glBindVertexArray(ground_.vao);
        glBindBuffer(GL_ARRAY_BUFFER, ground_.vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ground_.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
        ground_.count = (int)idx.size();
        glBindVertexArray(0);
    }
    std::vector<float> v;
    PlantBuilder b{v};
    const int FL = 11;
    auto mark = [&](int k, int lod) { meshFirst_[k][lod] = (int)(v.size() / FL); };
    auto done = [&](int k, int lod) { meshCount_[k][lod] = (int)(v.size() / FL) - meshFirst_[k][lod]; };
    for (int lod = 0; lod < 2; lod++) {
        const bool nearM = lod == 0;
        // grass: a tuft of blades
        mark(K_GRASS, lod);
        b.swayH = 0.9f; b.swayK = 0.6f;
        for (int i = 0; i < (nearM ? 11 : 5); i++) {
            const float a = b.r.range(0.f, TAU), l = b.r.range(0.45f, 0.95f), o = b.r.range(0.05f, 0.4f);
            const vec3 base(std::cos(a) * 0.08f, 0.f, std::sin(a) * 0.08f);
            b.blade(base, base + vec3(std::cos(a) * o, l, std::sin(a) * o), 0.06f, 0.045f, 3, vec3(0.f), 2);
        }
        done(K_GRASS, lod);
        // a flower: a stem, two leaves, one or two heads of petals around a heart
        mark(K_FLOWER, lod);
        b.swayH = 1.f; b.swayK = 0.7f;
        {
            const vec3 heads[2] = {vec3(0.f, 0.85f, 0.f), vec3(0.22f, 0.6f, 0.12f)};
            for (int h = 0; h < 2; h++) {
                const vec3 hc = heads[h];
                b.blade(vec3(0.f, 0.f, 0.f), hc, 0.f, 0.025f, 3, vec3(0.f), 2);
                b.blade(vec3(0.f, 0.f, 0.f), vec3(hc.z, hc.y, -hc.x) * 0.02f + hc, 0.f, 0.025f, 3, vec3(0.f), 1);
                const int n = 6;
                for (int i = 0; i < n; i++) {
                    const float a = TAU * i / n + h;
                    const vec3 d(std::cos(a), 0.25f, std::sin(a));
                    const vec3 s(-std::sin(a), 0.f, std::cos(a));
                    const vec3 tip = hc + d * (h == 0 ? 0.26f : 0.2f);
                    const vec3 mid = hc + d * 0.12f;
                    b.quad(hc, mid + s * 0.09f, tip, mid - s * 0.09f, hc, 4);
                }
                b.blob(hc + vec3(0.f, 0.02f, 0.f), vec3(0.065f), 0, 5);
            }
            b.blade(vec3(0.f), vec3(0.35f, 0.25f, 0.f), 0.12f, 0.07f, 3, vec3(0.f), 2);
            b.blade(vec3(0.f), vec3(-0.3f, 0.2f, 0.15f), 0.1f, 0.06f, 3, vec3(0.f), 2);
        }
        done(K_FLOWER, lod);
        // a fern: fronds arching out of the ground
        mark(K_FERN, lod);
        b.swayH = 1.f; b.swayK = 0.5f;
        for (int i = 0; i < (nearM ? 9 : 5); i++) {
            const float a = TAU * i / (nearM ? 9 : 5) + b.r.range(-0.2f, 0.2f);
            const vec3 d(std::cos(a), 0.f, std::sin(a));
            b.blade(vec3(0.f), d * b.r.range(0.9f, 1.3f) + vec3(0.f, 0.15f, 0.f), 0.75f, 0.17f, 8, vec3(0.f), 4);
        }
        done(K_FERN, lod);
        // a bush: a few round clumps of leaves
        mark(K_BUSH, lod);
        b.swayH = 1.6f; b.swayK = 0.3f;
        b.blob(vec3(0.f, 0.6f, 0.f), vec3(0.95f, 0.75f, 0.95f), nearM ? 1 : 0, 1);
        b.blob(vec3(0.65f, 0.45f, 0.3f), vec3(0.65f, 0.5f, 0.6f), nearM ? 1 : 0, 2);
        if (nearM) b.blob(vec3(-0.55f, 0.5f, -0.3f), vec3(0.7f, 0.55f, 0.7f), 1, 1);
        done(K_BUSH, lod);
        // a tree: a trunk and its branches under a round crown of clumps; vines hanging from it
        mark(K_TREE, lod);
        b.swayH = 7.5f; b.swayK = 1.f;
        b.cyl(vec3(0.f), vec3(0.15f, 3.7f, 0.05f), 0.3f, 0.19f, nearM ? 6 : 4, 0);
        if (nearM) {
            b.cyl(vec3(0.1f, 2.6f, 0.f), vec3(1.3f, 4.3f, 0.4f), 0.14f, 0.07f, 5, 0);
            b.cyl(vec3(-0.1f, 3.f, 0.f), vec3(-1.2f, 4.6f, -0.3f), 0.13f, 0.07f, 5, 0);
            b.cyl(vec3(0.1f, 3.3f, 0.f), vec3(0.2f, 5.f, -1.f), 0.12f, 0.06f, 5, 0);
            b.blob(vec3(0.2f, 5.3f, 0.f), vec3(2.f, 1.5f, 2.f), 1, 1);
            b.blob(vec3(1.5f, 4.7f, 0.5f), vec3(1.4f, 1.1f, 1.4f), 1, 2);
            b.blob(vec3(-1.4f, 5.f, -0.4f), vec3(1.5f, 1.2f, 1.5f), 1, 1);
            b.blob(vec3(0.3f, 4.7f, -1.3f), vec3(1.3f, 1.f, 1.3f), 1, 2);
            b.blob(vec3(0.f, 6.3f, 0.3f), vec3(1.3f, 1.f, 1.3f), 1, 2);
            const vec3 vt[6] = {vec3(1.7f, 4.1f, 0.6f), vec3(-1.6f, 4.3f, -0.3f), vec3(0.4f, 4.1f, -1.6f),
                                vec3(-0.6f, 4.2f, 1.2f), vec3(2.2f, 4.4f, -0.2f), vec3(-2.f, 4.5f, 0.8f)};
            const float vl[6] = {2.8f, 3.4f, 2.4f, 3.f, 2.2f, 2.6f};
            for (int i = 0; i < 6; i++) b.vine(vt[i], vl[i], 6);
        } else {
            b.blob(vec3(0.f, 5.1f, 0.f), vec3(2.3f, 1.8f, 2.3f), 0, 1);
            b.blob(vec3(0.9f, 4.5f, 0.6f), vec3(1.6f, 1.2f, 1.6f), 0, 2);
        }
        done(K_TREE, lod);
        // an emergent tree: a tall straight trunk, a wide flat crown high over the others
        mark(K_EMERGENT, lod);
        b.swayH = 12.f; b.swayK = 0.8f;
        b.cyl(vec3(0.f), vec3(0.f, 10.5f, 0.f), 0.45f, 0.22f, nearM ? 7 : 4, 0);
        if (nearM) {
            b.cyl(vec3(0.f, 9.5f, 0.f), vec3(2.1f, 10.7f, 0.7f), 0.16f, 0.08f, 5, 0);
            b.cyl(vec3(0.f, 9.7f, 0.f), vec3(-2.f, 10.8f, -0.6f), 0.16f, 0.08f, 5, 0);
            b.cyl(vec3(0.f, 9.8f, 0.f), vec3(0.4f, 10.6f, -2.f), 0.15f, 0.08f, 5, 0);
            b.blob(vec3(0.f, 11.3f, 0.f), vec3(3.2f, 1.2f, 3.f), 1, 1);
            b.blob(vec3(2.3f, 10.9f, 0.8f), vec3(2.f, 0.9f, 2.f), 1, 2);
            b.blob(vec3(-2.2f, 11.f, -0.6f), vec3(2.1f, 0.9f, 2.f), 1, 1);
            b.blob(vec3(0.4f, 10.8f, -2.1f), vec3(1.8f, 0.8f, 1.8f), 1, 2);
            const vec3 vt[5] = {vec3(2.4f, 10.3f, 0.8f), vec3(-2.3f, 10.4f, -0.5f), vec3(0.5f, 10.2f, -2.f), vec3(-1.f, 10.3f, 1.8f),
                                vec3(3.f, 10.5f, -0.6f)};
            const float vl[5] = {4.5f, 5.5f, 4.f, 5.f, 3.5f};
            for (int i = 0; i < 5; i++) b.vine(vt[i], vl[i], 6);
        } else {
            b.blob(vec3(0.f, 11.f, 0.f), vec3(3.4f, 1.3f, 3.2f), 0, 1);
            b.blob(vec3(1.8f, 10.6f, 0.6f), vec3(2.f, 0.9f, 2.f), 0, 2);
        }
        done(K_EMERGENT, lod);
        // a palm: a curved trunk, a crown of long arching fronds
        mark(K_PALM, lod);
        b.swayH = 7.f; b.swayK = 0.9f;
        {
            const int segs = nearM ? 6 : 3;
            auto at = [&](float t) { return vec3(0.9f * t * t, 6.4f * t, 0.15f * t); };
            for (int i = 0; i < segs; i++) {
                const float t0 = (float)i / segs, t1 = (float)(i + 1) / segs;
                b.cyl(at(t0), at(t1), 0.26f - 0.08f * t0, 0.26f - 0.08f * t1, nearM ? 6 : 4, 0);
            }
            const vec3 crown = at(1.f);
            const int n = nearM ? 9 : 6;
            for (int i = 0; i < n; i++) {
                const float a = TAU * i / n + 0.3f;
                const vec3 d(std::cos(a), 0.f, std::sin(a));
                b.blade(crown, crown + d * 2.7f + vec3(0.f, -1.2f, 0.f), 1.1f, 0.42f, 8, crown, nearM ? 4 : 2);
            }
        }
        done(K_PALM, lod);
        // a dead tree: a bare grey trunk and branches (leaf clumps at their tips, grown back when the land revives)
        mark(K_DEAD, lod);
        b.swayH = 5.5f; b.swayK = 0.25f;
        {
            b.cyl(vec3(0.f), vec3(0.1f, 4.2f, 0.f), 0.28f, 0.13f, nearM ? 6 : 4, 7);
            const vec3 br[5][2] = {{vec3(0.f, 2.4f, 0.f), vec3(1.4f, 3.8f, 0.3f)}, {vec3(0.f, 3.f, 0.f), vec3(-1.3f, 4.4f, -0.2f)},
                                   {vec3(0.05f, 3.6f, 0.f), vec3(0.4f, 5.4f, 0.5f)}, {vec3(0.05f, 3.4f, 0.f), vec3(-0.2f, 5.2f, -0.9f)},
                                   {vec3(0.9f, 3.3f, 0.2f), vec3(1.8f, 4.4f, -0.4f)}};
            for (int i = 0; i < 5; i++) {
                b.cyl(br[i][0], br[i][1], 0.11f, 0.035f, nearM ? 5 : 3, 7);
                b.blob(br[i][1], vec3(0.9f, 0.7f, 0.9f), nearM ? 1 : 0, i % 2 ? 2 : 1);
            }
        }
        done(K_DEAD, lod);
    }
    glGenVertexArrays(1, &plantVao_);
    glGenBuffers(1, &plantVbo_);
    glGenBuffers(1, &plantInstVbo_);
    glBindVertexArray(plantVao_);
    glBindBuffer(GL_ARRAY_BUFFER, plantVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, FL * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, FL * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, FL * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, FL * sizeof(float), (void*)(9 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, plantInstVbo_);
    for (int i = 0; i < 3; i++) {
        glEnableVertexAttribArray(4 + i);
        glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(4 + i, 1);
    }
    glBindVertexArray(0);
}

// Glyphs baked as signed distance fields (they scale cleanly).
void RewildUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] rewild font init failed\n");
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
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, ch, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, ch, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
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

void RewildUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = v2(hw, hh);
    viewC_ = v2(cam.target.x, cam.target.y);
    camDist_ = cam.dist;
    px_ = 2.f * hh / height_;
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bgT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
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
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) std::fprintf(stderr, "[gl] rewild world FBO incomplete\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    setCamera();
}

RewildUniverse::~RewildUniverse() {
    sceneT_.destroy();
    bgT_.destroy();
    for (GLuint p : {progSky_, progGround_, progPlant_, progDepth_, progBird_, progPart_, progRain_, progVeil_, progCell_, progSprite_,
                     progText_, progShape_, progPost_})
        glDeleteProgram(p);
    glDeleteVertexArrays(1, &ground_.vao);
    glDeleteBuffers(1, &ground_.vbo);
    glDeleteBuffers(1, &ground_.ibo);
    for (GLuint v : {plantVao_, cellVao_, spriteVao_, birdVao_, partVao_, textVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {plantVbo_, plantInstVbo_, quadVbo_, cellVbo_, spriteVbo_, birdVbo_, partVbo_, textVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
    glDeleteTextures(1, &lifeTexId_);
    glDeleteFramebuffers(1, &msFbo_);
    glDeleteRenderbuffers(1, &msColor_);
    glDeleteRenderbuffers(1, &msDepth_);
    glDeleteFramebuffers(1, &shadowFbo_);
    glDeleteTextures(1, &shadowTex_);
    for (Font* f : {&fontItalic_, &fontRoman_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Camera, light
// =====================================================================================================

// The layout's reference camera, raised above the land and looking straight ahead, its image shifted back down: the
// board plane lands on the reference camera's pixels.
void RewildUniverse::setCamera() {
    const layout::Camera ref = layout::camera(width_, height_);
    const float aspect = (float)width_ / height_;
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    const float f = 1.f / std::tan(half);
    eye_ = vec3(ref.target.x, EYE_Y, ref.target.z + ref.dist);
    horizon_ = f * (EYE_Y - ref.target.y) / ref.dist;
    view_ = mat4::lookAt(eye_, eye_ + vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f));
    mat4 shift = mat4::identity();
    shift.at(1, 3) = horizon_;
    vp_ = shift * mat4::perspective(2.f * half, aspect, 1.f, 900.f) * view_;
    invVP_ = inverse(vp_);
    camR_ = vec3(1.f, 0.f, 0.f);
    camU_ = vec3(0.f, 1.f, 0.f);
}

vec3 RewildUniverse::sunDir() const {
    const layout::Camera ref = layout::camera(width_, height_);
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    float el = pal_.sunEl;
    if (el < 0.f) el = std::atan(-el * (1.f - horizon_) * std::tan(half)) * 180.f / PI;
    const float a = pal_.sunAz * PI / 180.f, e = el * PI / 180.f;
    return normalize(vec3(std::sin(a) * std::cos(e), std::sin(e), -std::cos(a) * std::cos(e)));
}

void RewildUniverse::worldUniforms(GLuint p) {
    const float bridge = saturate(bridge_.amount);
    const float world = 1.f - smoothstepf(0.f, 0.6f, bridge);
    set3f(p, "uEye", eye_);
    set1f(p, "uFrontZ", FRONT_Z);
    set1f(p, "uTime", time_);
    set1f(p, "uSlow", slow_);
    set1f(p, "uNight", pal_.night);
    set1f(p, "uFlash", flash_ * world);
    set1f(p, "uCloudT", cloudT_);
    set1f(p, "uStars", pal_.stars);
    set1f(p, "uClouds", pal_.clouds);
    set1f(p, "uLife", life_);
    set1f(p, "uRain", rain_ * world);
    set1f(p, "uWet", wet_);
    set3f(p, "uSunDir", sunDir());
    // the light: the sun's, a steady glow per section (brighter at a peak), dimmed by the rain's overcast and the dust
    const float wp = smoothstepf(1.1f, 2.f, lv_), calm = 1.f - smoothstepf(0.f, 1.f, lv_);
    const float glow = (1.f + 0.12f * wp - 0.1f * calm) * (1.f - 0.7f * rain_) * (0.9f + 0.1f * life_);
    set3f(p, "uSunCol", pal_.sun * (pal_.lightK * glow));
    const vec3 amb = lerp(pal_.skyTop, pal_.skyLow, 0.45f) * pal_.ambK;
    set3f(p, "uAmb", lerp(amb, pal_.stormSky * 1.5f, rain_ * 0.5f));
    set4f(p, "uSunDisc", pal_.sunR * 1.65f * PI / 180.f, 0.f, pal_.moon, 1.f - rain_);
    set3f(p, "uSkyTop", pal_.skyTop);
    set3f(p, "uSkyLow", pal_.skyLow);
    set3f(p, "uDust", pal_.dust);
    set3f(p, "uMist", pal_.mist);
    set3f(p, "uDusk", pal_.dusk);
    set3f(p, "uStormSky", pal_.stormSky);
    set3f(p, "uStormCloud", pal_.stormCloud);
    set3f(p, "uCloud", pal_.cloud);
    {
        const float a = flashAz_ * PI / 180.f, e = 7.f * PI / 180.f;
        set3f(p, "uFlashDir", vec3(std::sin(a) * std::cos(e), std::sin(e), -std::cos(a) * std::cos(e)));
    }
    set3f(p, "uLifeArea", vec3(LIFE_X0, LIFE_Z0, LIFE_RES * LIFE_CELL));
    set1i(p, "uLifeMap", 6);
    setMat(p, "uVP", vp_);
}

// The plants, one instanced draw per kind and detail (the far forest simpler); `shadow`: into the shadow map (the
// near ones only).
void RewildUniverse::drawPlants(GLuint prog, bool shadow) {
    (void)prog;
    if (plants_.empty()) return;
    glBindVertexArray(plantVao_);
    glBindBuffer(GL_ARRAY_BUFFER, plantInstVbo_);
    static std::vector<float> inst[K_COUNT][2];
    for (int k = 0; k < K_COUNT; k++)
        for (int l = 0; l < 2; l++) inst[k][l].clear();
    for (const Plant& p : plants_) {
        if (p.g <= 0.f) continue;
        if (shadow && p.p.z < -60.f) continue;
        const float reach = 6.f * p.scale + 2.f;
        if (std::fabs(p.p.x) > hwAt(p.p.z) + reach) continue;
        const int lod = p.p.z < -45.f ? 1 : 0;
        const float v[12] = {p.p.x, p.p.y, p.p.z, p.scale, p.rot, p.g, p.seed, p.vines,
                             p.tint, p.wither > 0.f ? std::min(1.f, p.wither * 0.6f) : 0.f, (float)p.kind, 0.f};
        inst[p.kind][lod].insert(inst[p.kind][lod].end(), v, v + 12);
    }
    for (int k = 0; k < K_COUNT; k++)
        for (int l = 0; l < 2; l++) {
            const std::vector<float>& in = inst[k][l];
            if (in.empty()) continue;
            glBufferData(GL_ARRAY_BUFFER, in.size() * sizeof(float), in.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLES, meshFirst_[k][l], meshCount_[k][l], (GLsizei)(in.size() / 12));
        }
}

// =====================================================================================================
// The stack's cubes
// =====================================================================================================

void RewildUniverse::cube(float x, float y, float size, int type, float alpha, bool ghost, float seed, float crack) {
    if (alpha <= 0.003f) return;
    type = std::clamp(type, 0, 6);
    const vec3 col = pal_.piece[type];
    cells_.push_back({{x, y, size, (float)type}, {col.x, col.y, col.z, alpha}, {ghost ? 1.f : 0.f, crack, 0.f, 0.f}, {seed, 0.f, 0.f, 0.f}});
}

void RewildUniverse::flushCells() {
    if (cells_.empty()) return;
    glUseProgram(progCell_);
    setMat(progCell_, "uVP", vp_);
    set1f(progCell_, "uPx", px_);
    set1f(progCell_, "uTime", time_);
    set1f(progCell_, "uBridge", smoothstepf(0.3f, 0.95f, bridge_.amount));
    set3f(progCell_, "uBridgeInk", bridge_.ink);
    set2f(progCell_, "uRes", (float)width_, (float)height_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bgT_.tex);
    set1i(progCell_, "uBg", 0);
    // the light the seeds see: from the sun's side, high
    const vec3 L = sunDir();
    set2f(progCell_, "uSunP", L.x * 14.f, 6.f + std::max(L.y, 0.2f) * 10.f);
    set3f(progCell_, "uSunCol", pal_.sun * (0.9f + 0.2f * pal_.lightK) * (1.f - 0.35f * rain_));
    set3f(progCell_, "uSkyTop", pal_.skyTop);
    set3f(progCell_, "uSkyLow", pal_.skyLow);
    set3f(progCell_, "uSoil", pal_.soilDry);
    set1f(progCell_, "uNight", pal_.night);
    std::stable_sort(cells_.begin(), cells_.end(), [](const CellInst& a, const CellInst& b) {
        const float ra = std::round(a.a0[1] * 4.f), rb = std::round(b.a0[1] * 4.f);
        if (ra != rb) return ra < rb;
        return std::fabs(a.a0[0]) > std::fabs(b.a0[0]);
    });
    glBindVertexArray(cellVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    cells_.clear();
}

// =====================================================================================================
// Text
// =====================================================================================================

float RewildUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv * k + track;
        if (i + 1 < s.size()) {
            unsigned char n = (unsigned char)s[i + 1];
            if (n < 32 || n >= 128) n = '?';
            w += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
    return w - track;
}

float RewildUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                                std::vector<float>& out) const {
    if (!f.ok) return 0.f;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
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
        if (i + 1 < s.size()) {
            unsigned char n = (unsigned char)s[i + 1];
            if (n < 32 || n >= 128) n = '?';
            pen += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
    return pen - track - x;
}

void RewildUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float bias) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount);
    const vec3 c = lerp(ink, bridge_.ink, b);
    // a soft halo of the other value under the letters
    const bool light = ugl::luma(c) > 0.3f;
    set3f(progText_, "uInk", c);
    set3f(progText_, "uHalo", light ? vec3(0.015f, 0.012f, 0.01f) : lerp(pal_.skyLow, vec3(1.f), 0.5f));
    set1f(progText_, "uHaloA", (light ? 0.42f : 0.35f) * (1.f - b));
    set1f(progText_, "uAlpha", alpha);
    set1f(progText_, "uBias", bias);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// How light the world is behind a point (board space): the sky above the horizon, the land below it (its soil or its
// green), the well's pane.
float RewildUniverse::bgLuma(float bx, float by) const {
    const float ndc = (by - viewC_.y) / viewH_.y;
    vec3 c;
    if (by < groundH(bx, FRONT_Z)) c = pal_.soilCut * 0.7f;
    else if (ndc < horizon_) {
        const vec3 green = lerp(pal_.grassLush, pal_.leafA, 0.6f);
        c = lerp(lerp(pal_.soilDry, pal_.soilWet, wet_ * 0.7f), green, life_ * 0.85f);
        c = lerp(c, lerp(pal_.dust, pal_.mist, life_), 0.25f + 0.4f * smoothstepf(horizon_ - 0.3f, horizon_, ndc));
    } else {
        const float h = saturate((ndc - horizon_) * 0.344f / 0.3f);
        c = lerp(pal_.skyLow, pal_.skyTop, smoothstepf(0.f, 1.f, std::pow(h, 0.7f)));
        c = lerp(c, pal_.stormSky, rain_ * 0.8f);
    }
    c = lerp(c, pal_.dusk, slow_ * 0.6f);
    if (std::fabs(bx) < 5.1f && std::fabs(by) < 10.1f) c = lerp(c, pal_.panel, pal_.panelA);
    return ugl::luma(c);
}

// The HUD in REWILD's typography: Lora, a book face like a naturalist's field notes; italic for the song, the section
// and the celebrations, spaced capitals for labels. Ink dark over a pale sky or bare clay, light over the dark well,
// the jungle and the night, always on a soft halo.
void RewildUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    if (!panels.empty()) {
        std::vector<float> v;
        auto quad = [&](float x0, float y0, float x1, float y1, const vec3& c, float a) {
            const float q[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                   {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& r : q) v.insert(v.end(), r, r + 6);
        };
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? ugl::toLinear(r.color) : r.accent ? pal_.accent : pal_.panel;
            quad(r.x, r.y, r.x + r.w, r.y + r.h, c, std::min(1.f, r.alpha));
        }
        glUseProgram(progShape_);
        set2f(progShape_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(shapeVao_);
        glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 6));
    }
    const bool inkDark = ugl::luma(pal_.ink) < 0.2f;
    const vec3 darkInk = inkDark ? pal_.ink : ugl::toLinear(hex(0x231a12));
    const vec3 lightInk = inkDark ? ugl::toLinear(hex(0xf6f0e4)) : pal_.ink;
    std::vector<float> q;
    for (const HudText& h : hud) {
        if (h.alpha <= 0.01f || h.text.empty()) continue;
        const float base = h.scale * 12.f;
        auto inkAt = [&](float px, float py) {
            const float bx = viewC_.x + (px / width_ * 2.f - 1.f) * viewH_.x;
            const float by = viewC_.y - (py / height_ * 2.f - 1.f) * viewH_.y;
            const float l = (std::sqrt(bgLuma(bx, by - 0.3f)) + std::sqrt(bgLuma(bx, by)) + std::sqrt(bgLuma(bx, by + 0.3f))) / 3.f;
            return std::fabs(l - std::sqrt(ugl::luma(darkInk))) > std::fabs(l - std::sqrt(ugl::luma(lightInk))) ? darkInk : lightInk;
        };
        vec3 ink = inkAt(h.x < 0.f ? width_ + h.x : h.x, h.y + base * 0.5f);
        vec3 accent = lerp(pal_.accent, ink, 0.25f);
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            const float x = ugl::hudLeft(h, w, width_);
            ink = inkAt(x + w * 0.5f, h.y + dy + size * 0.8f);
            accent = lerp(pal_.accent, ink, 0.25f);
            textQuads(f, text, x, h.y + dy, size, track, q);
        };
        auto titleCase = [](const std::string& str) {
            std::string o = str;
            bool start = true;
            for (char& c : o) {
                if (std::isalpha((unsigned char)c)) {
                    c = start ? (char)std::toupper((unsigned char)c) : (char)std::tolower((unsigned char)c);
                    start = false;
                } else start = c == ' ' || c == '-';
            }
            return o;
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            // "Rewild" in the italic, the variant's name in small spaced capitals after it.
            std::string name = h.text, var;
            const size_t dot = name.find("\xC2\xB7");
            if (dot != std::string::npos) { var = name.substr(dot + 2); name = name.substr(0, dot); }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!var.empty() && var.front() == ' ') var.erase(var.begin());
            name = titleCase(name);
            const float sz = base * 1.4f, vsz = base * 0.64f, vtr = vsz * 0.3f;
            const float wName = textWidth(fontItalic_, name, sz, 0.f);
            const float wVar = var.empty() ? 0.f : textWidth(fontRoman_, var, vsz, vtr);
            const float gap = base * 0.8f;
            const float x = ugl::hudLeft(h, wName + (var.empty() ? 0.f : gap + wVar), width_);
            textQuads(fontItalic_, name, x, h.y - base * 0.12f, sz, 0.f, q);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            drawText(fontItalic_, q, inkAt(x + wName * 0.5f, h.y + sz * 0.4f), a, 0.03f);
            if (!var.empty()) {
                q.clear();
                textQuads(fontRoman_, var, x + wName + gap, h.y + base * 0.22f, vsz, vtr, q);
                drawText(fontRoman_, q, inkAt(x + wName + gap + wVar * 0.5f, h.y + base * 0.22f + vsz * 0.5f), a * 0.85f, 0.02f);
            }
            continue;
        }
        case HUD_SECTION:
            place(fontItalic_, titleCase(h.text), base * 1.35f, 0.f);
            drawText(fontItalic_, q, ink, saturate(0.2f + h.alpha * 1.9f), 0.03f);
            continue;
        case HUD_TITLE:
            place(fontItalic_, h.text, base * 1.25f, 0.f, -base * 0.12f);
            drawText(fontItalic_, q, ink, saturate(h.alpha * 1.3f), 0.03f);
            continue;
        case HUD_INFO:
            place(fontRoman_, h.text, base * 0.78f, base * 0.12f);
            drawText(fontRoman_, q, ink, saturate(alpha * 1.2f) * 0.85f, 0.02f);
            continue;
        case HUD_LABEL:
            place(fontRoman_, h.text, base * 0.72f, base * 0.3f);
            drawText(fontRoman_, q, ink, saturate(alpha * 1.2f) * 0.78f, 0.02f);
            continue;
        case HUD_VALUE:
            place(fontRoman_, h.text, base * 1.08f, base * 0.04f);
            drawText(fontRoman_, q, ink, alpha, 0.02f);
            continue;
        case HUD_NOTICE:
            place(fontItalic_, h.text, base * 1.15f, 0.f);
            drawText(fontItalic_, q, accent, alpha, 0.03f);
            continue;
        case HUD_KEY:
            place(fontRoman_, h.text, base * 0.88f, base * 0.05f);
            drawText(fontRoman_, q, ink, alpha, 0.02f);
            continue;
        case HUD_TOAST:
            place(fontItalic_, titleCase(h.text), base * 1.5f, 0.f, -base * 0.2f);
            drawText(fontItalic_, q, ink, alpha, 0.04f);
            continue;
        case HUD_TOAST_SUB:
            place(fontRoman_, h.text, base * 0.82f, base * 0.2f);
            drawText(fontRoman_, q, accent, alpha * 0.9f, 0.02f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontItalic_, titleCase(h.text), base * 1.3f, 0.f, -base * 0.12f);
                drawText(fontItalic_, q, ink, alpha, 0.04f);
            } else {
                place(fontRoman_, h.text, base * 0.86f, base * 0.1f);
                drawText(fontRoman_, q, h.accent ? accent : ink, alpha, 0.02f);
            }
            continue;
        default:
            place(fontRoman_, h.text, base * 0.84f, base * 0.06f);
            drawText(fontRoman_, q, h.accent ? accent : ink, alpha, 0.02f);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void RewildUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { pal_ = target; palSet_ = true; }
        else lerpPalette(pal_, target, 1.f - std::exp(-dt * 1.0f));
    }
    if (!started_) {
        plantStumps();
        started_ = true;
    }
    // the section, from the HUD (a drop is the rain, a chorus the sun)
    sectionKnown_ = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown_ = true;
            sectionDrop_ = h.text == "DROP";
        }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 0.9f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (over_ && !game.over()) over_ = false;
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, slow_);
    if (!f.paused) {
        time_ += dt;
        watchBoard(game);
        simulate(game, music, dt, wdt);
    }
    const float bridge = saturate(bridge_.amount);
    const float world = 1.f - smoothstepf(0.f, 0.6f, bridge);
    setCamera();

    // ---- The life map, uploaded when it changed (a few times a second at most).
    lifeUpload_ -= dt;
    if (lifeDirty_ && lifeUpload_ <= 0.f) {
        lifeUpload_ = 0.15f;
        lifeDirty_ = false;
        for (int i = 0; i < LIFE_RES * LIFE_RES; i++) {
            lifeTex_[i * 4 + 0] = (unsigned char)(saturate(lifeG_[i]) * 255.f);
            lifeTex_[i * 4 + 1] = (unsigned char)(saturate(lifeM_[i]) * 255.f);
            lifeTex_[i * 4 + 2] = (unsigned char)(saturate(lifeF_[i]) * 255.f);
            lifeTex_[i * 4 + 3] = 255;
        }
        glBindTexture(GL_TEXTURE_2D, lifeTexId_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, LIFE_RES, LIFE_RES, GL_RGBA, GL_UNSIGNED_BYTE, lifeTex_.data());
    }
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, lifeTexId_);
    glActiveTexture(GL_TEXTURE0);

    auto plantUniforms = [&](GLuint p) {
        set1f(p, "uSwayT", swayT_);
        set1f(p, "uWind", wind_);
        set1f(p, "uGust", gust_ * world);
        set1f(p, "uBloom", bloom_);
        setMat(p, "uVP", vp_);
    };

    // ---- The sun's shadow map: the plants near the well.
    {
        const vec3 L = sunDir();
        const vec3 c(0.f, -6.f, -26.f);
        const vec3 up = std::fabs(L.y) > 0.95f ? vec3(0, 0, 1) : vec3(0, 1, 0);
        lightVP_ = mat4::ortho(-48.f, 48.f, -40.f, 40.f, 1.f, 300.f) * mat4::lookAt(c + L * 140.f, c, up);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, SHADOW_RES, SHADOW_RES);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 2.f);
    glUseProgram(progDepth_);
    plantUniforms(progDepth_);
    setMat(progDepth_, "uVP", lightVP_);
    drawPlants(progDepth_, true);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // ---- The world.
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClear(GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glActiveTexture(GL_TEXTURE0);
    auto shadowUniforms = [&](GLuint p) {
        set1i(p, "uShadow", 5);
        setMat(p, "uLightVP", lightVP_);
        set1f(p, "uShadowK", pal_.shadowK * (1.f - 0.75f * rain_));
    };
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(progSky_);
    worldUniforms(progSky_);
    setMat(progSky_, "uInvVP", invVP_);
    ugl::drawFullscreen();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    // the land
    glUseProgram(progGround_);
    worldUniforms(progGround_);
    shadowUniforms(progGround_);
    set3f(progGround_, "uSoilDry", pal_.soilDry);
    set3f(progGround_, "uSoilWet", pal_.soilWet);
    set3f(progGround_, "uSoilCut", pal_.soilCut);
    set3f(progGround_, "uCrack", pal_.crack);
    set3f(progGround_, "uGrassDry", pal_.grassDry);
    set3f(progGround_, "uGrassLush", pal_.grassLush);
    set3f(progGround_, "uLeafFar", pal_.leafFar);
    set3f(progGround_, "uFlowerA", pal_.flowerA);
    set3f(progGround_, "uFlowerB", pal_.flowerB);
    set1f(progGround_, "uPx", px_);
    set1f(progGround_, "uBloom", bloom_);
    glBindVertexArray(ground_.vao);
    glDrawElements(GL_TRIANGLES, ground_.count, GL_UNSIGNED_INT, nullptr);
    // the plants
    if (!plants_.empty()) {
        glUseProgram(progPlant_);
        worldUniforms(progPlant_);
        shadowUniforms(progPlant_);
        plantUniforms(progPlant_);
        set3f(progPlant_, "uBark", pal_.bark);
        set3f(progPlant_, "uLeafA", pal_.leafA);
        set3f(progPlant_, "uLeafB", pal_.leafB);
        set3f(progPlant_, "uGrassDry", pal_.grassDry);
        set3f(progPlant_, "uGrassLush", pal_.grassLush);
        set3f(progPlant_, "uDead", pal_.dead);
        set3f(progPlant_, "uFlowerC", lerp(pal_.flowerB, vec3(1.f, 0.85f, 0.3f), 0.5f));
        set3f(progPlant_, "uLeafFar", pal_.leafFar);
        float pc[21];
        for (int i = 0; i < 7; i++) {
            const vec3 c = lerp(pal_.piece[i], pal_.flowerA, 0.15f);
            pc[i * 3] = c.x; pc[i * 3 + 1] = c.y; pc[i * 3 + 2] = c.z;
        }
        glUniform3fv(ugl::U(progPlant_, "uPieces"), 7, pc);
        drawPlants(progPlant_, false);
    }
    // the birds, the butterflies, the fireflies (premultiplied)
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    if (!birds_.empty() && world > 0.01f) {
        std::vector<float> inst;
        for (const Bird& b : birds_) {
            const float a = b.alpha * world;
            if (a <= 0.003f) continue;
            const float dir = b.v.x >= 0.f ? 1.f : -1.f;
            const float v[16] = {b.p.x, b.p.y, b.p.z, b.size, (float)b.kind, b.flap, dir, a, b.col.x, b.col.y, b.col.z, 0.f, 0.f, 0.f, 0.f, 0.f};
            inst.insert(inst.end(), v, v + 16);
        }
        if (!inst.empty()) {
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(progBird_);
            worldUniforms(progBird_);
            set3f(progBird_, "uCamR", camR_);
            set3f(progBird_, "uCamU", camU_);
            set1f(progBird_, "uPx", px_);
            glBindVertexArray(birdVao_);
            glBindBuffer(GL_ARRAY_BUFFER, birdVbo_);
            glBufferData(GL_ARRAY_BUFFER, inst.size() * sizeof(float), inst.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(inst.size() / 16));
        }
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // the particles: the dust, the splashes and the pollen in the world; the seeds and the glints of glass after the
    // well's pane (they leave the well bright)
    std::vector<float> back, front;
    for (const Particle& p : parts_) {
        if (p.age < 0.f) continue;
        const float t = p.age / p.life;
        const float fade = 1.f - smoothstepf(0.6f, 1.f, t);
        auto put = [&](std::vector<float>& out, float size, int kind, const vec3& col, float alpha, float stretch, float soft, float ang) {
            if (alpha * world <= 0.003f) return;
            const float v[16] = {p.p.x, p.p.y, p.p.z, size, p.v.x, p.v.y, p.v.z, (float)kind, col.x, col.y, col.z, alpha * world,
                                 stretch, soft, p.seed, ang};
            out.insert(out.end(), v, v + 16);
        };
        switch (p.kind) {
        case P_SEED: put(front, p.size, 1, p.col, 1.f, 0.f, 0.f, p.spin * p.age + p.seed * 6.f); break;
        case P_SHARD: put(front, p.size, 2, lerp(pal_.skyLow, vec3(1.f), 0.7f), 0.9f * (1.f - t), 0.f, 0.f, p.spin * p.age + p.seed * 6.f); break;
        case P_DUST: put(back, p.size * (1.f + 1.5f * t), 5, p.col, 0.45f * (1.f - t), 0.f, 0.f, 0.f); break;
        case P_SPLASH: put(back, p.size, 0, p.col, 0.7f * fade, 0.25f, 0.5f, 0.f); break;
        case P_MOTE: put(back, p.size, 0, p.col, 0.55f * smoothstepf(0.f, 0.15f, t) * fade, 0.f, 0.8f, 0.f); break;
        case P_PETAL: put(back, p.size, 1, p.col, fade, 0.f, 0.f, p.spin * p.age); break;
        default: break;
        }
    }
    auto drawParts = [&](const std::vector<float>& v, bool aerialOn) {
        if (v.empty()) return;
        glUseProgram(progPart_);
        worldUniforms(progPart_);
        set3f(progPart_, "uCamR", camR_);
        set3f(progPart_, "uCamU", camU_);
        set1f(progPart_, "uPx", px_);
        set1f(progPart_, "uAerial", aerialOn ? 1.f : 0.f);
        glBindVertexArray(partVao_);
        glBindBuffer(GL_ARRAY_BUFFER, partVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(v.size() / 16));
    };
    drawParts(back, true);
    // the rain slanting across
    if (rain_ * world > 0.01f) {
        glDisable(GL_DEPTH_TEST);
        glUseProgram(progRain_);
        set4f(progRain_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set1f(progRain_, "uPx", px_);
        set1f(progRain_, "uRainA", rain_ * world);
        set1f(progRain_, "uTimeR", time_);
        set3f(progRain_, "uRainCol", lerp(pal_.mist, vec3(1.f), 0.4f));
        ugl::drawFullscreen();
        glEnable(GL_DEPTH_TEST);
    }
    // ---- The well's pane over the world (and the bridge: the world fading to its color).
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progVeil_);
    set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set1f(progVeil_, "uPx", px_);
    set1f(progVeil_, "uPanelA", pal_.panelA * (1.f - bridge));
    set1f(progVeil_, "uBridge", smoothstepf(0.05f, 0.85f, bridge));
    set3f(progVeil_, "uPanel", pal_.panel);
    set3f(progVeil_, "uEdge", lerp(pal_.skyLow, vec3(1.f), 0.3f));
    set3f(progVeil_, "uBridgeCol", bridge_.color);
    ugl::drawFullscreen();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // the seeds flying out and the glints (hidden by the plants in front of them, never veiled)
    glEnable(GL_DEPTH_TEST);
    drawParts(front, false);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    // ---- A copy of the scene so far: what the stack's glass shows behind it.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneT_.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bgT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    sceneT_.bind();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // ---- The ghost, the stack, the falling piece: every cell its own glass cube of seeds.
    if (game.hasPiece() && !game.over()) {
        const Piece& p = game.piece();
        Piece gp = p;
        gp.y = game.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            game.pieceCells(gp, gc);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cube(w.x, w.y, 1.f, p.type, 1.f, true, 0.f);
            }
        }
    }
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const int t = game.cell(x, y).type;
            if (t < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - game.rowOffset(y));
            cube(p.x, p.y, 1.f, t, 1.f, false, std::fmod((float)(x * 7 + y * 13 + t), 50.f));
        }
    for (const Dying& d : dying_) cube(d.x, d.y, 1.f, d.type, 1.f, false, d.x * 3.f + d.y, smoothstepf(d.delay - 0.15f, d.delay, d.t));
    if (game.hasPiece() && !game.over()) {
        const Piece& p = game.piece();
        int cells[4][2];
        game.pieceCells(p, cells);
        const float dx = game.pieceVisualX() - p.x, dy = game.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            const auto& c = cells[i];
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cube(w.x, w.y, 1.f, p.type, 1.f, false, (float)(p.type * 7 + i * 13));
        }
    }
    auto preview = [&](int type, vec2 center, float sc, float alpha) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        game.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        for (int i = 0; i < 4; i++) {
            const auto& c = cells[i];
            const vec2 pos = v2(center.x + (c[0] - (minx + maxx) * 0.5f) * sc, center.y - (c[1] - (miny + maxy) * 0.5f) * sc);
            cube(pos.x, pos.y, sc, type, alpha, false, (float)(type * 3 + i * 11));
        }
    };
    preview(game.holdType(), v2(layout::HOLD_CENTER.x, layout::HOLD_CENTER.y), layout::previewScale(0), game.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(game.next(i), v2(c.x, c.y), layout::previewScale(i), 1.f);
    }
    flushCells();
    // ---- The puffs of dust under a lock (at the contact, small).
    if (!puffs_.empty()) {
        std::vector<float> pv;
        const vec3 dc = lerp(pal_.soilDry, pal_.dust, 0.5f);
        for (const Puff& q : puffs_) {
            const float t = q.age / q.life;
            const float v[16] = {q.p.x, q.p.y, 0.f, q.size * (1.f + 1.2f * t), q.v.x, q.v.y, 0.f, 5.f, dc.x, dc.y, dc.z,
                                 0.5f * (1.f - t) * world, 0.f, 0.f, 0.f, 0.f};
            pv.insert(pv.end(), v, v + 16);
        }
        drawParts(pv, false);
    }
    // ---- The bonus gauge: a round glass filling with seeds.
    {
        const vec3 gc = layout::GAUGE_CENTER;
        const float R = layout::GAUGE_R;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / 10.f) : saturate(gaugeShown_);
        const float ready = game.bonusReady() || game.bonusActive() ? 1.f : 0.f;
        const vec3 col = pal_.piece[3];
        const float v[16] = {gc.x, gc.y, R, R, 0.f, 0.f, fill, ready, col.x, col.y, col.z, 1.f - 0.6f * smoothstepf(0.3f, 0.95f, bridge), 0.f, 0.f, 0.f, 0.f};
        glUseProgram(progSprite_);
        setMat(progSprite_, "uVP", vp_);
        set1f(progSprite_, "uTime", time_);
        set1f(progSprite_, "uPx", px_);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 1);
    }
    drawHud(f.hud, f.panels);
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
