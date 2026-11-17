// BIOME's sea, its drawing (see sea.cpp): the GL resources, the world's meshes (the sandy bed and its cut face, the
// sea's surface, the boats), its passes.
//
// The shadow map: the thrown cubes, the boats; from the same view, the light through the cubes' water (their colored
// shadows). The world, multisampled, with depth: the sky, the sand (its puddles), the boats, the life in the water; a
// copy of it (what the thrown cubes refract); the thrown cubes (ray traced, writing their depth), the particles under
// the water, the sea's surface, the water's cut face, the particles above it; the lens (the bloom, the sun's glare,
// the well's dark glass pane and the panels). The shell then adds the rain, the bridge and the stack.
#include <algorithm>
#include <cstdio>

#include "layout.hpp"
#include "universes/biome/gl.hpp"
#include "universes/biome/sea.hpp"
#include "universes/biome/sea_shaders.hpp"
#include "universes/biome/shaders.hpp"

namespace biome {

using namespace sea;
namespace shaders = sea_shaders;
namespace common = biome_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

namespace {


// Low-poly meshes: position, normal, material (the boats' paints: 0 hull, 1 superstructure, 2 sail, 3 accent,
// 4 windows, 5..7 containers, 8 dark, 9 deck).
struct MeshBuilder {
    std::vector<float>& v;
    void tri(const vec3& a, const vec3& b, const vec3& c, int m) {
        const vec3 n = normalize(cross(b - a, c - a));
        for (const vec3* p : {&a, &b, &c}) v.insert(v.end(), {p->x, p->y, p->z, n.x, n.y, n.z, (float)m});
    }
    void quad(const vec3& a, const vec3& b, const vec3& c, const vec3& d, int m) {
        tri(a, b, c, m);
        tri(a, c, d, m);
    }
    void box(const vec3& lo, const vec3& hi, int m) {
        const vec3 p[8] = {vec3(lo.x, lo.y, lo.z), vec3(hi.x, lo.y, lo.z), vec3(hi.x, hi.y, lo.z), vec3(lo.x, hi.y, lo.z),
                           vec3(lo.x, lo.y, hi.z), vec3(hi.x, lo.y, hi.z), vec3(hi.x, hi.y, hi.z), vec3(lo.x, hi.y, hi.z)};
        quad(p[0], p[3], p[2], p[1], m); // back (z-)
        quad(p[4], p[5], p[6], p[7], m); // front (z+)
        quad(p[0], p[4], p[7], p[3], m); // left
        quad(p[1], p[2], p[6], p[5], m); // right
        quad(p[3], p[7], p[6], p[2], m); // top
        quad(p[0], p[1], p[5], p[4], m); // bottom
    }
    // a hull along x (bow at +x): length, beam, depth under the water, freeboard; its sides above the water in `side`,
    // below in the dark paint, the deck in `deck`
    void hull(float L, float B, float D, float F, int side, int deck) {
        const int N = 12;
        auto W = [&](float u) { return u < 0.6f ? B * 0.5f * (0.82f + 0.18f * u / 0.6f) : B * 0.5f * std::sqrt(std::max(0.f, 1.f - (u - 0.6f) * (u - 0.6f) / 0.16f)); };
        auto Fh = [&](float u) { return F * (1.f + 0.3f * std::max(0.f, u - 0.7f) / 0.3f); };
        auto Dh = [&](float u) { return -D * (1.f - 0.65f * std::max(0.f, u - 0.65f) / 0.35f); };
        const float wl = std::min(F * 0.35f, 0.25f);
        for (int i = 0; i < N; i++) {
            const float u0 = (float)i / N, u1 = (float)(i + 1) / N;
            const float x0 = -L * 0.5f + L * u0, x1 = -L * 0.5f + L * u1;
            const float w0 = W(u0), w1 = W(u1), f0 = Fh(u0), f1 = Fh(u1), d0 = Dh(u0), d1 = Dh(u1);
            const float b0 = w0 * 0.45f, b1 = w1 * 0.45f;
            // the waterline's width along the side (between the deck's and the bottom's)
            auto at = [&](float w, float b, float f, float d, float y) { return b + (w - b) * (y - d) / std::max(f - d, 1e-3f); };
            const float m0 = at(w0, b0, f0, d0, wl), m1 = at(w1, b1, f1, d1, wl);
            for (float s : {1.f, -1.f}) {
                quad(vec3(x0, f0, s * w0), vec3(x1, f1, s * w1), vec3(x1, wl, s * m1), vec3(x0, wl, s * m0), side);
                quad(vec3(x0, wl, s * m0), vec3(x1, wl, s * m1), vec3(x1, d1, s * b1), vec3(x0, d0, s * b0), 8);
            }
            quad(vec3(x0, d0, b0), vec3(x1, d1, b1), vec3(x1, d1, -b1), vec3(x0, d0, -b0), 8);
            quad(vec3(x0, f0, -w0), vec3(x1, f1, -w1), vec3(x1, f1, w1), vec3(x0, f0, w0), deck);
        }
        const float x0 = -L * 0.5f, w0 = W(0.f), f0 = Fh(0.f), d0 = Dh(0.f);
        quad(vec3(x0, f0, -w0), vec3(x0, f0, w0), vec3(x0, d0, w0 * 0.45f), vec3(x0, d0, -w0 * 0.45f), side);
    }
    // a band of windows around a block (slightly proud of it)
    void windows(float x0, float x1, float y, float h, float zw) {
        box(vec3(x0 + 0.08f, y - h * 0.5f, -zw - 0.02f), vec3(x1 + 0.02f, y + h * 0.5f, zw + 0.02f), 4);
    }
};

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool SeaWorld::init() {
    const char* W = common::SKY_GLSL;
    const char* S = common::SHADOW_GLSL;
    const char* G = common::GLASS_GLSL;
    progSky_ = compile(ugl::FULLSCREEN_VS, shaders::SKY_FS, "sea sky", {W, shaders::WORLD_GLSL});
    progSand_ = compile(shaders::SAND_VS, shaders::SAND_FS, "sea sand", {W, shaders::WORLD_GLSL, S});
    progSurf_ = compile(shaders::SURF_VS, shaders::SURF_FS, "sea surface", {W, shaders::WORLD_GLSL});
    progFront_ = compile(shaders::FRONT_VS, shaders::FRONT_FS, "sea front", {W, shaders::WORLD_GLSL});
    progBoat_ = compile(shaders::MESH_VS, shaders::BOAT_FS, "sea boat", {W, shaders::WORLD_GLSL, S});
    progDepth_ = compile(shaders::MESH_VS, common::DEPTH_FS, "sea depth", {});
    progPass_ = compile(shaders::MESH_VS, shaders::PASS_FS, "sea light through the cubes", {});
    passT_.create(1024, 1024, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
    glGenTextures(1, &pudTex_);
    glBindTexture(GL_TEXTURE_2D, pudTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, PUD_W, PUD_H, 0, GL_RG, GL_FLOAT, pud_.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    progDown_ = compile(ugl::FULLSCREEN_VS, shaders::LENS_DOWN_FS, "sea lens down", {});
    progBlur_ = compile(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "sea lens blur", {});
    progLens_ = compile(ugl::FULLSCREEN_VS, shaders::LENS_FS, "sea lens", {});
    progCube_ = compile(shaders::CUBE_VS, shaders::CUBE_FS, "sea thrown cube", {W, shaders::WORLD_GLSL, G, shaders::WATER_GLSL});
    progLife_ = compile(shaders::LIFE_VS, shaders::LIFE_FS, "sea life", {W, shaders::WORLD_GLSL});
    progPart_ = compile(common::PART_VS, common::PART_FS, "sea particle", {W, shaders::WORLD_GLSL});
    progCell_ = compile(common::CELL_VS, std::string(common::CELL_HEAD) + shaders::CELL_FILL + common::CELL_MAIN, "sea cube",
                        {G, shaders::WATER_GLSL});
    ctx.instanced(cubeVao_, cubeVbo_, 5);
    ctx.instanced(lifeVao_, lifeVbo_, 5);
    buildMeshes();
    return glGetError() == GL_NO_ERROR;
}

// The world's meshes: the sandy bed (a grid denser near the well, wider far out, its cut face along the front), the
// sea's surface (a grid lifted by the swell in its vertex shader), the water's cut face (a strip along the front), the
// boats and a box for the cubes' shadows.
void SeaWorld::buildMeshes() {
    auto upload = [](Mesh& m, const std::vector<float>& v, const std::vector<unsigned>& idx, const std::vector<int>& comps) {
        glGenVertexArrays(1, &m.vao);
        glGenBuffers(1, &m.vbo);
        glBindVertexArray(m.vao);
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
        int stride = 0;
        for (int c : comps) stride += c;
        int off = 0;
        for (size_t i = 0; i < comps.size(); i++) {
            glEnableVertexAttribArray((GLuint)i);
            glVertexAttribPointer((GLuint)i, comps[i], GL_FLOAT, GL_FALSE, stride * (GLsizei)sizeof(float), (void*)(sizeof(float) * off));
            off += comps[i];
        }
        if (!idx.empty()) {
            glGenBuffers(1, &m.ibo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
            m.count = (int)idx.size();
        } else m.count = (int)(v.size() / stride);
        glBindVertexArray(0);
    };
    auto grid = [](int cols, int rows, std::vector<unsigned>& idx, unsigned base) {
        for (int j = 0; j < rows; j++)
            for (int i = 0; i < cols; i++) {
                const unsigned a = base + j * (cols + 1) + i, b = a + 1, c = a + cols + 1, d = c + 1;
                idx.insert(idx.end(), {a, c, b, b, c, d});
            }
    };
    {
        // the sand: z from the front out to 800 (as far as the sea's surface: a dry start is sand to the horizon), x
        // widening with the distance; then the cut face
        std::vector<float> v;
        std::vector<unsigned> idx;
        const int C = 256, R = 256;
        for (int j = 0; j <= R; j++) {
            const float t = (float)j / R;
            const float z = FRONT_Z - 800.f * std::pow(t, 2.5f);
            const float hw = 32.f + 0.95f * (-z);
            for (int i = 0; i <= C; i++) {
                const float x = ((float)i / C * 2.f - 1.f) * hw;
                const vec3 n = sandN(x, z);
                v.insert(v.end(), {x, sandH(x, z), z, n.x, n.y, n.z});
            }
        }
        grid(C, R, idx, 0);
        const unsigned base = (unsigned)(v.size() / 6);
        const int CF = 400;
        for (int i = 0; i <= CF; i++) {
            const float x = ((float)i / CF * 2.f - 1.f) * 70.f;
            v.insert(v.end(), {x, -40.f, FRONT_Z, 0.f, 0.f, 1.f});
            v.insert(v.end(), {x, sandH(x, FRONT_Z), FRONT_Z, 0.f, 0.f, 1.f});
        }
        for (int i = 0; i < CF; i++) {
            const unsigned a = base + i * 2, b = a + 1, c = a + 2, d = a + 3;
            idx.insert(idx.end(), {a, c, b, b, c, d});
        }
        upload(sand_, v, idx, {3, 3});
    }
    {
        // the sea's surface: out to 800 (the haze takes it far before)
        std::vector<float> v;
        std::vector<unsigned> idx;
        const int C = 220, R = 190;
        for (int j = 0; j <= R; j++) {
            const float t = (float)j / R;
            const float z = FRONT_Z - 800.f * std::pow(t, 2.6f);
            const float hw = 32.f + 0.95f * (-z);
            for (int i = 0; i <= C; i++) v.insert(v.end(), {((float)i / C * 2.f - 1.f) * hw, z});
        }
        grid(C, R, idx, 0);
        upload(surf_, v, idx, {2});
    }
    {
        std::vector<float> v;
        const int CF = 480;
        for (int i = 0; i <= CF; i++) {
            const float x = ((float)i / CF * 2.f - 1.f) * 70.f;
            v.insert(v.end(), {x, 0.f, x, 1.f});
        }
        upload(front_, v, {}, {2});
        front_.count = (CF + 1) * 2;
    }
    {
        // the boats (bow at +x, the waterline at y = 0) and the cubes' box
        std::vector<float> v;
        MeshBuilder b{v};
        auto mark = [&](int t) { boatFirst_[t] = (int)(v.size() / 7); };
        auto done = [&](int t) { boatCount_[t] = (int)(v.size() / 7) - boatFirst_[t]; };
        // a dinghy: a small hull, a mast, one sail
        mark(0);
        b.hull(1.6f, 0.62f, 0.15f, 0.22f, 0, 9);
        b.box(vec3(0.12f, 0.2f, -0.025f), vec3(0.17f, 1.55f, 0.025f), 8);
        b.tri(vec3(0.1f, 0.32f, 0.f), vec3(0.1f, 1.5f, 0.f), vec3(-0.6f, 0.34f, 0.f), 2);
        done(0);
        // a sailboat: a cabin, a tall mast, a mainsail and a jib
        mark(1);
        b.hull(3.2f, 1.0f, 0.3f, 0.38f, 0, 9);
        b.box(vec3(-0.7f, 0.36f, -0.3f), vec3(0.4f, 0.62f, 0.3f), 1);
        b.windows(-0.6f, 0.3f, 0.5f, 0.08f, 0.3f);
        b.box(vec3(0.25f, 0.3f, -0.035f), vec3(0.32f, 4.1f, 0.035f), 8);
        b.tri(vec3(0.22f, 0.72f, 0.f), vec3(0.22f, 3.95f, 0.f), vec3(-1.35f, 0.74f, 0.f), 2);
        b.tri(vec3(0.36f, 0.8f, 0.f), vec3(0.36f, 3.55f, 0.f), vec3(1.55f, 0.45f, 0.f), 2);
        b.box(vec3(-1.4f, 0.38f, -0.5f), vec3(1.5f, 0.42f, 0.5f), 3);
        done(1);
        // a fishing boat: a red hull, a wheelhouse aft, a mast and its boom
        mark(2);
        b.hull(4.6f, 1.6f, 0.5f, 0.6f, 3, 9);
        b.box(vec3(-1.5f, 0.58f, -0.55f), vec3(-0.4f, 1.55f, 0.55f), 1);
        b.windows(-1.45f, -0.4f, 1.3f, 0.22f, 0.55f);
        b.box(vec3(-1.55f, 1.55f, -0.6f), vec3(-0.35f, 1.62f, 0.6f), 8);
        b.box(vec3(0.75f, 0.6f, -0.05f), vec3(0.85f, 3.0f, 0.05f), 8);
        b.box(vec3(0.8f, 1.3f, -0.03f), vec3(2.0f, 1.36f, 0.03f), 8);
        b.box(vec3(-0.2f, 0.6f, -0.5f), vec3(0.5f, 0.85f, 0.5f), 5);
        done(2);
        // a ferry: two decks of windows, a bridge, a funnel
        mark(3);
        b.hull(10.f, 2.6f, 0.8f, 1.0f, 0, 9);
        b.box(vec3(-4.2f, 0.98f, -1.2f), vec3(3.2f, 1.95f, 1.2f), 1);
        b.windows(-4.1f, 3.2f, 1.45f, 0.3f, 1.2f);
        b.box(vec3(-3.4f, 1.95f, -1.0f), vec3(1.8f, 2.75f, 1.0f), 1);
        b.windows(-3.3f, 1.8f, 2.35f, 0.28f, 1.0f);
        b.box(vec3(0.6f, 2.75f, -0.8f), vec3(1.8f, 3.25f, 0.8f), 1);
        b.windows(0.6f, 1.8f, 3.05f, 0.2f, 0.8f);
        b.box(vec3(-1.9f, 2.75f, -0.38f), vec3(-0.9f, 3.9f, 0.38f), 3);
        b.box(vec3(-1.95f, 3.9f, -0.42f), vec3(-0.85f, 4.05f, 0.42f), 8);
        done(3);
        // a container ship: rows of containers, the bridge tower aft
        mark(4);
        b.hull(18.f, 3.0f, 1.0f, 1.2f, 0, 8);
        {
            Rng r(77);
            for (int i = 0; i < 6; i++) {
                const float x0 = -4.6f + i * 2.25f;
                for (int k = 0; k < 2; k++) {
                    const int hgt = r.irange(1, 3);
                    for (int l = 0; l < hgt; l++) {
                        const float z0 = k == 0 ? -1.35f : 0.05f;
                        b.box(vec3(x0, 1.2f + l * 0.72f, z0), vec3(x0 + 2.1f, 1.2f + (l + 1) * 0.72f - 0.04f, z0 + 1.3f), 5 + r.irange(0, 2));
                    }
                }
            }
        }
        b.box(vec3(-8.3f, 1.18f, -1.4f), vec3(-6.2f, 4.6f, 1.4f), 1);
        b.windows(-8.2f, -6.2f, 4.15f, 0.35f, 1.4f);
        b.box(vec3(-8.6f, 4.6f, -1.6f), vec3(-6.0f, 4.75f, 1.6f), 8);
        b.box(vec3(-7.6f, 4.75f, -0.35f), vec3(-6.9f, 5.6f, 0.35f), 3);
        done(4);
        // a liner: three tiers of windows, two funnels
        mark(5);
        b.hull(22.f, 3.2f, 1.0f, 1.6f, 0, 9);
        b.box(vec3(-9.2f, 1.58f, -1.5f), vec3(7.2f, 2.6f, 1.5f), 1);
        b.windows(-9.1f, 7.2f, 2.1f, 0.32f, 1.5f);
        b.box(vec3(-8.2f, 2.6f, -1.35f), vec3(5.6f, 3.5f, 1.35f), 1);
        b.windows(-8.1f, 5.6f, 3.05f, 0.3f, 1.35f);
        b.box(vec3(-6.6f, 3.5f, -1.15f), vec3(4.0f, 4.3f, 1.15f), 1);
        b.windows(-6.5f, 4.0f, 3.9f, 0.26f, 1.15f);
        b.box(vec3(2.6f, 4.3f, -0.9f), vec3(4.0f, 4.8f, 0.9f), 1);
        for (float fx : {-3.8f, -0.8f}) {
            b.box(vec3(fx, 4.3f, -0.55f), vec3(fx + 1.4f, 6.2f, 0.55f), 3);
            b.box(vec3(fx - 0.05f, 6.2f, -0.6f), vec3(fx + 1.45f, 6.45f, 0.6f), 8);
        }
        done(5);
        boxFirst_ = (int)(v.size() / 7);
        b.box(vec3(-0.44f), vec3(0.44f), 0);
        boxCount_ = (int)(v.size() / 7) - boxFirst_;

        glGenVertexArrays(1, &boatVao_);
        glGenBuffers(1, &meshVbo_);
        glGenBuffers(1, &meshInstVbo_);
        glBindVertexArray(boatVao_);
        glBindBuffer(GL_ARRAY_BUFFER, meshVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, meshInstVbo_);
        for (int i = 0; i < 5; i++) {
            glEnableVertexAttribArray(3 + i);
            glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, 20 * sizeof(float), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(3 + i, 1);
        }
        glBindVertexArray(0);
        boxVao_ = boatVao_;
    }
}

SeaWorld::~SeaWorld() {
    for (GLuint p : {progSky_, progSand_, progSurf_, progFront_, progBoat_, progDepth_, progPass_, progCube_, progLife_, progPart_, progCell_,
                    progDown_, progBlur_, progLens_})
        glDeleteProgram(p);
    passT_.destroy();
    glDeleteTextures(1, &pudTex_);
    for (ugl::Target* t : {&quarterT_[0], &quarterT_[1], &eighthT_[0], &eighthT_[1]}) t->destroy();
    for (Mesh* m : {&sand_, &surf_, &front_}) {
        glDeleteVertexArrays(1, &m->vao);
        glDeleteBuffers(1, &m->vbo);
        if (m->ibo) glDeleteBuffers(1, &m->ibo);
    }
    for (GLuint v : {boatVao_, cubeVao_, lifeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {meshVbo_, meshInstVbo_, cubeVbo_, lifeVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Uniforms, meshes
// =====================================================================================================

// The world's uniforms (SKY_GLSL, WORLD_GLSL).
void SeaWorld::worldUniforms(GLuint p) {
    const float world = ctx.world();
    // the sun dimmed by the storm's overcast; the clouds heavier in a storm
    ctx.skyUniforms(p, 1.f - 0.75f * ctx.storm, ctx.pal.clouds + 0.55f * ctx.storm, v2(0.004f, 0.05f), 0.24f, 0.f);
    set1f(p, "uSea", seaLevel());
    set1f(p, "uWaveT", waveT_);
    set1f(p, "uAmp", amp_);
    set1f(p, "uSharp", sharp_);
    set1f(p, "uDir", dir_);
    set1f(p, "uCaps", caps_ * world);
    set3f(p, "uHaze", ctx.pal.haze);
    set3f(p, "uWater", ctx.pal.sea2);
    set3f(p, "uDeep", ctx.pal.deep);
    set3f(p, "uFoam", ctx.pal.foam);
    set3f(p, "uStormSea", ctx.pal.stormSea);
    set1f(p, "uAirDust", ctx.pal.airDust);
    set1f(p, "uWind", dir_);
}

// The boats, one instanced draw per type (depthOnly: into the shadow map).
void SeaWorld::drawBoats(bool depthOnly) {
    if (boats_.empty()) return;
    glBindVertexArray(boatVao_);
    glBindBuffer(GL_ARRAY_BUFFER, meshInstVbo_);
    std::vector<float> inst;
    for (int t = 0; t < 6; t++) {
        inst.clear();
        for (const Boat& b : boats_) {
            if (b.type != t || b.alpha <= 0.01f) continue;
            if (depthOnly && b.z < -40.f) continue;
            const float L = BOAT_LEN[t] * b.scale, B = L * 0.15f + 0.3f;
            const float bow = b.x + b.dir * L / 3.f, stern = b.x - b.dir * L / 3.f;
            const float pitch = std::atan((waveH(bow, b.z) - waveH(stern, b.z)) / (2.f * L / 3.f)) * 0.8f;
            const float roll = std::atan((waveH(b.x, b.z + B) - waveH(b.x, b.z - B)) / (2.f * B)) * 0.6f +
                               rock_ * std::sin(ctx.time * 0.85f + b.phase) / (1.f + t * 0.4f);
            // the boat's frame: its length along its course, pitched by the swell, rolled across it
            const vec3 fx0(b.dir, 0.f, 0.f), fz0(0.f, 0.f, b.dir);
            const float cp = std::cos(pitch), sp = std::sin(pitch), cr = std::cos(roll), sr = std::sin(roll);
            const vec3 fx = fx0 * cp + vec3(0.f, sp, 0.f);
            const vec3 fy1 = vec3(0.f, cp, 0.f) - fx0 * sp;
            const vec3 fy = fy1 * cr + fz0 * sr;
            const vec3 fz = fz0 * cr - fy1 * sr;
            const vec3 pos(b.x, surfY(b.x, b.z) - 0.05f * L * 0.1f, b.z);
            const mat4 m = model(pos, fx, fy, fz, b.scale);
            inst.insert(inst.end(), m.m, m.m + 16);
            inst.insert(inst.end(), {b.alpha * (1.f - smoothstepf(0.f, 0.6f, saturate(ctx.bridge.amount))), b.seed, 0.f, 0.f});
        }
        if (inst.empty()) continue;
        glBufferData(GL_ARRAY_BUFFER, inst.size() * sizeof(float), inst.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLES, boatFirst_[t], boatCount_[t], (GLsizei)(inst.size() / 20));
    }
}

// The thrown cubes as boxes (their shadows), each with the color of the light its water lets through, its fill.
void SeaWorld::drawBoxes() {
    std::vector<float> inst;
    for (const Body& b : bodies_) {
        if (b.age < b.delay || b.fade > 0.6f || ctx.bare) continue;
        const mat4 m = model(b.p, qRot(b.q, vec3(1, 0, 0)), qRot(b.q, vec3(0, 1, 0)), qRot(b.q, vec3(0, 0, 1)), 1.f);
        const vec3 c = ctx.pal.cell[b.type];
        inst.insert(inst.end(), m.m, m.m + 16);
        inst.insert(inst.end(), {c.x, c.y, c.z, b.fill});
    }
    if (inst.empty()) return;
    glBindVertexArray(boxVao_);
    glBindBuffer(GL_ARRAY_BUFFER, meshInstVbo_);
    glBufferData(GL_ARRAY_BUFFER, inst.size() * sizeof(float), inst.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLES, boxFirst_, boxCount_, (GLsizei)(inst.size() / 20));
}

// =====================================================================================================
// Passes
// =====================================================================================================

// The sun's shadow map: the thrown cubes and the near boats on the sand around the well. Then, from the same view, the
// light the cubes' water lets through (their colored shadows, brighter where it is focused; nothing through a boat).
void SeaWorld::shadowPass() {
    const vec3 L = ctx.sunDir();
    const vec3 c(0.f, -9.f, -14.f);
    const vec3 up = std::fabs(L.y) > 0.95f ? vec3(0, 0, 1) : vec3(0, 1, 0);
    ctx.lightVP = mat4::ortho(-30.f, 30.f, -30.f, 30.f, 1.f, 300.f) * mat4::lookAt(c + L * 140.f, c, up);
    glUseProgram(progDepth_);
    setMat(progDepth_, "uVP", ctx.lightVP);
    drawBoxes();
    drawBoats(true);

    GLint shadowFbo = 0, vp[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &shadowFbo);
    glGetIntegerv(GL_VIEWPORT, vp);
    passT_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(progPass_);
    setMat(progPass_, "uVP", ctx.lightVP);
    set1f(progPass_, "uTime", ctx.time);
    drawBoxes();
    glUseProgram(progDepth_);
    drawBoats(true);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)shadowFbo);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
}

void SeaWorld::drawWorld() {
    const float world = ctx.world();
    auto shadowUniforms = [&](GLuint p) {
        set1i(p, "uShadow", 5);
        setMat(p, "uLightVP", ctx.lightVP);
        set1f(p, "uShadowK", ctx.pal.shadowK * (1.f - 0.7f * ctx.storm));
    };
    // the sky (and the water's cut face where nothing is behind it)
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(progSky_);
    worldUniforms(progSky_);
    setMat(progSky_, "uInvVP", ctx.invVP);
    ugl::drawFullscreen();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    // the sandy bed
    glUseProgram(progSand_);
    worldUniforms(progSand_);
    shadowUniforms(progSand_);
    set3f(progSand_, "uSand", ctx.pal.sand);
    {
        // the cubes lying on the sand (or about to), nearest the eye first: their contact shadows
        std::vector<const Body*> near;
        for (const Body& b : bodies_)
            if (b.age >= b.delay && b.fade < 1.f && !ctx.bare && b.p.y - sandH(b.p.x, b.p.z) < 1.4f) near.push_back(&b);
        std::sort(near.begin(), near.end(), [](const Body* a, const Body* b) { return a->p.z > b->p.z; });
        float bv[24 * 4];
        const int n = std::min((int)near.size(), 24);
        for (int i = 0; i < n; i++) {
            const Body& b = *near[i];
            const float v[4] = {b.p.x, b.p.y, b.p.z, (1.f - smoothstepf(0.f, 1.f, b.fade)) * world};
            std::copy(v, v + 4, bv + i * 4);
        }
        if (n > 0) glUniform4fv(ugl::U(progSand_, "uBodies"), n, bv);
        set1i(progSand_, "uBodyN", n);
    }
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, passT_.tex);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, pudTex_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, PUD_W, PUD_H, GL_RG, GL_FLOAT, pud_.data());
    glActiveTexture(GL_TEXTURE0);
    set1i(progSand_, "uPass", 6);
    set1i(progSand_, "uPud", 7);
    set4f(progSand_, "uPudRect", -PUD_X, PUD_Z0, 1.f / (2.f * PUD_X), 1.f / (PUD_Z1 - PUD_Z0));
    glBindVertexArray(sand_.vao);
    glDrawElements(GL_TRIANGLES, sand_.count, GL_UNSIGNED_INT, nullptr);
    // the boats
    if (!boats_.empty()) {
        glUseProgram(progBoat_);
        worldUniforms(progBoat_);
        shadowUniforms(progBoat_);
        const vec3 mats[10] = {ctx.pal.hull, ctx.pal.super_, ctx.pal.sail, ctx.pal.baccent, ctx.pal.super_ * 0.25f, ctx.pal.cont0, ctx.pal.cont1, ctx.pal.cont2,
                               ctx.pal.hull * 0.45f + vec3(0.02f), lerp(ctx.pal.super_, ctx.pal.hull, 0.5f) * 0.8f};
        float mf[30];
        for (int i = 0; i < 10; i++) { mf[i * 3] = mats[i].x; mf[i * 3 + 1] = mats[i].y; mf[i * 3 + 2] = mats[i].z; }
        glUniform3fv(ugl::U(progBoat_, "uMat"), 10, mf);
        set3f(progBoat_, "uLit", ctx.pal.window);
        drawBoats(false);
    }
    // the life in the water (premultiplied, under the surface)
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    if (!critters_.empty() && world > 0.01f) {
        std::vector<float> inst;
        const float S = seaLevel();
        for (const Critter& c : critters_) {
            const float a = c.alpha * world;
            if (a <= 0.003f) continue;
            const float y = critterY(c);
            const float dark = smoothstepf(0.5f, 10.f, S - y);
            vec3 body = c.col;
            float hx = c.size, hy = c.size, glow = 0.f, anim = ctx.time + c.phase;
            switch (c.kind) {
            case L_FISH:
                body = lerp(c.col, ctx.pal.deep * 0.25f, dark * 0.7f);
                hx = c.size * 1.25f; hy = c.size * 0.7f;
                anim = ctx.time * (6.f + 6.f * c.dart) + c.phase;
                break;
            case L_JELLY:
                hx = c.size * 1.2f; hy = c.size * 2.6f;
                glow = 0.35f + 0.35f * dark + 0.6f * glowUp_;
                anim = ctx.worldTime * 0.55f + c.phase;
                break;
            case L_ANGLER:
                hx = c.size * 1.6f; hy = c.size * 1.25f;
                glow = 0.8f + 0.4f * glowUp_;
                anim = ctx.worldTime + c.phase;
                break;
            case L_SUB:
                // (wider than the hull, for the headlight's beam)
                hx = c.size * 1.6f; hy = c.size * 0.65f;
                glow = 0.7f + 0.3f * dark + 0.4f * glowUp_;
                anim = ctx.worldTime + c.phase;
                break;
            case L_SIPHON:
                hx = c.size + 0.3f; hy = 0.7f;
                glow = 0.55f + 0.5f * glowUp_;
                anim = ctx.worldTime * 0.35f + c.phase;
                break;
            default: {
                const float tw = 0.55f + 0.45f * std::sin(ctx.time * (0.3f + 0.02f * c.seed) + c.seed * 6.f);
                glow = (0.8f + 0.5f * glowUp_) * tw * smoothstepf(0.35f, 0.8f, dark);
                hx = hy = c.size;
                break;
            }
            }
            const float bob = c.kind == L_JELLY ? 0.25f * std::sin(anim * 0.5f) : c.kind == L_FISH ? 0.04f * std::sin(ctx.time * 1.3f + c.phase * 3.f) : c.kind == L_SUB ? 0.08f * std::sin(anim * 0.4f) : 0.f;
            const float v[20] = {c.x, y + bob, hx, hy, c.dir, (float)c.kind, anim, glow, body.x, body.y, body.z, a,
                                 c.glow.x, c.glow.y, c.glow.z, c.seed, c.z, 0.f, 0.f, 0.f};
            inst.insert(inst.end(), v, v + 20);
        }
        if (!inst.empty()) {
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(progLife_);
            worldUniforms(progLife_);
            set1f(progLife_, "uPx", ctx.px);
            set1f(progLife_, "uLifeGlow", 0.5f + 0.5f * glowUp_ + 0.5f);
            glBindVertexArray(lifeVao_);
            glBindBuffer(GL_ARRAY_BUFFER, lifeVbo_);
            glBufferData(GL_ARRAY_BUFFER, inst.size() * sizeof(float), inst.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(inst.size() / 20));
        }
    }
    // a copy of the world so far: what the thrown cubes' glass shows behind it
    glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx.msFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx.bgT.fbo);
    glBlitFramebuffer(0, 0, ctx.width, ctx.height, 0, 0, ctx.width, ctx.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx.msFbo);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // the thrown cubes, far to near, each writing its depth (not in a break's frame: the game's cubes are hidden)
    if (!bodies_.empty() && !ctx.bare) {
        std::vector<const Body*> order;
        for (const Body& b : bodies_)
            if (b.age >= b.delay) order.push_back(&b);
        std::sort(order.begin(), order.end(), [&](const Body* a, const Body* b) {
            const vec3 da = a->p - ctx.eye, db = b->p - ctx.eye;
            return dot(da, da) > dot(db, db);
        });
        std::vector<float> inst;
        for (const Body* b : order) {
            const vec3 col = ctx.pal.cell[b->type];
            const float alpha = (1.f - smoothstepf(0.f, 1.f, b->fade)) * world;
            const float v[20] = {b->p.x, b->p.y, b->p.z, 1.f, b->q[0], b->q[1], b->q[2], b->q[3], col.x, col.y, col.z, alpha,
                                 b->crackL.x, b->crackL.y, b->crackL.z, b->crack, b->fill, b->seed, 0.f, 0.f};
            inst.insert(inst.end(), v, v + 20);
        }
        if (!inst.empty()) {
            glDepthMask(GL_TRUE);
            glUseProgram(progCube_);
            worldUniforms(progCube_);
            set3f(progCube_, "uCamR", ctx.camR);
            set3f(progCube_, "uCamU", ctx.camU);
            set1f(progCube_, "uLightPanel", ctx.pal.lightPanel);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ctx.bgT.tex);
            set1i(progCube_, "uBg", 0);
            glBindVertexArray(cubeVao_);
            glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
            glBufferData(GL_ARRAY_BUFFER, inst.size() * sizeof(float), inst.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(inst.size() / 20));
            glDepthMask(GL_FALSE);
        }
    }
    // the particles: those under the water before the surface, those above it after
    std::vector<float> under, above;
    // what lights them in the air (the sun, the sky)
    const vec3 lit = ctx.pal.sun * (0.55f * ctx.pal.lightK * (1.f - 0.6f * ctx.storm)) +
                     lerp(ctx.pal.skyTop, ctx.pal.skyLow, 0.5f) * ctx.pal.ambK * 0.9f;
    for (const Particle& p : parts_) {
        if (p.age < 0.f) continue;
        const float t = p.age / p.life;
        const float a = (1.f - smoothstepf(0.6f, 1.f, t)) * world;
        std::vector<float>& out = p.p.y < surfY(p.p.x, p.p.z) - 0.03f ? under : above;
        auto put = [&](float size, int kind, const vec3& col, float alpha, float stretch, float soft) {
            if (alpha <= 0.003f) return;
            const float v[16] = {p.p.x, p.p.y, p.p.z, size, p.v.x, p.v.y, p.v.z, (float)kind, col.x, col.y, col.z, alpha,
                                 stretch, soft, p.seed, 0.f};
            out.insert(out.end(), v, v + 16);
        };
        switch (p.kind) {
        case P_JET:
            // one soft body for the whole jet (it reads over a pale sky as over the sand), its drops bright over it
            put(p.size * 1.5f, 0, lerp(WATER, ctx.pal.sea2, 0.5f) * lit, 0.3f * world, 0.18f, 0.8f);
            put(p.size, 0, lerp(p.col, vec3(1.f), 0.35f) * lit, 0.85f * world, 0.18f, 0.6f);
            break;
        case P_RUN: put(p.size * 1.1f, 0, p.col, 0.6f * (1.f - t) * world, 0.5f, 0.6f); break;
        case P_SPLASH: put(p.size, 4, p.col * lit, a * 0.95f, 0.1f, 0.f); break;
        case P_DUST: put(p.size * (1.f + 2.f * t), 5, p.col * lit, 0.35f * (1.f - t) * world, 0.f, 0.f); break;
        case P_SPRAY: put(p.size, 0, p.col, a * 0.7f, 0.12f, 0.7f); break;
        case P_BUBBLE: put(p.size, 3, p.col, a * 0.9f, 0.f, 0.f); break;
        case P_PLUME: put(p.size * (1.f + 1.6f * t), 5, p.col, 0.5f * (1.f - t) * world, 0.f, 0.f); break;
        case P_SAND: put(p.size, 0, p.col, a, 0.f, 0.4f); break;
        default: break;
        }
    }
    auto drawParts = [&](const std::vector<float>& v) {
        if (v.empty()) return;
        glUseProgram(progPart_);
        worldUniforms(progPart_);
        set1f(progPart_, "uAerial", 1.f);
        ctx.drawParts(progPart_, v);
    };
    drawParts(under);
    // the sea's surface (it writes its depth: what is above it is drawn after)
    glDepthMask(GL_TRUE);
    glUseProgram(progSurf_);
    worldUniforms(progSurf_);
    set1f(progSurf_, "uGlint", glint_ * world);
    glBindVertexArray(surf_.vao);
    glDrawElements(GL_TRIANGLES, surf_.count, GL_UNSIGNED_INT, nullptr);
    glDepthMask(GL_FALSE);
    // the water's cut face: a fine lit line at its top
    glUseProgram(progFront_);
    worldUniforms(progFront_);
    set1f(progFront_, "uPx", ctx.px);
    glBindVertexArray(front_.vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, front_.count);
    drawParts(above);
    drawLens();
}

// The lens, over the world drawn so far (before the shell's rain and the stack): the world taken down and blurred (what
// the frosted glass shows) and its brightest light blurred wide (the bloom); then laid back over it with the sun's
// glare and the dark frosted glass: the well's pane (a fine grid of its cells etched in it, a bright rim) and the panels
// of the held and next pieces. The stack, the previews and the HUD are laid over it after: none of it touches them.
void SeaWorld::drawLens() {
    const int qw = std::max(1, ctx.width / 4), qh = std::max(1, ctx.height / 4);
    if (quarterT_[0].w != qw || quarterT_[0].h != qh) {
        for (ugl::Target& t : quarterT_) t.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
        for (ugl::Target& t : eighthT_) t.create(std::max(1, qw / 2), std::max(1, qh / 2), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    }
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx.msFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx.bgT.fbo);
    glBlitFramebuffer(0, 0, ctx.width, ctx.height, 0, 0, ctx.width, ctx.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glActiveTexture(GL_TEXTURE0);
    auto down = [&](const ugl::Target& src, const ugl::Target& dst, bool bright) {
        dst.bind();
        glUseProgram(progDown_);
        glBindTexture(GL_TEXTURE_2D, src.tex);
        set1i(progDown_, "uSrc", 0);
        set2f(progDown_, "uTexel", 1.f / src.w, 1.f / src.h);
        set1f(progDown_, "uBright", bright ? 1.f : 0.f);
        ugl::drawFullscreen();
    };
    auto blur = [&](ugl::Target* t, float spread) {
        glUseProgram(progBlur_);
        set1i(progBlur_, "uSrc", 0);
        for (int pass = 0; pass < 2; pass++) {
            t[1 - pass].bind();
            glBindTexture(GL_TEXTURE_2D, t[pass].tex);
            set2f(progBlur_, "uDir", pass == 0 ? spread / t[0].w : 0.f, pass == 0 ? 0.f : spread / t[0].h);
            ugl::drawFullscreen();
        }
    };
    down(ctx.bgT, quarterT_[0], false);
    blur(quarterT_, 0.6f);
    down(quarterT_[0], eighthT_[0], true);
    blur(eighthT_, 2.f);
    blur(eighthT_, 2.f);

    glBindFramebuffer(GL_FRAMEBUFFER, ctx.msFbo);
    glViewport(0, 0, ctx.width, ctx.height);
    const GLuint p = progLens_;
    glUseProgram(p);
    glBindTexture(GL_TEXTURE_2D, ctx.bgT.tex);
    set1i(p, "uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, quarterT_[0].tex);
    set1i(p, "uFrost", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, eighthT_[0].tex);
    set1i(p, "uBloom", 2);
    glActiveTexture(GL_TEXTURE0);
    const Palette& pal = ctx.pal;
    const float world = ctx.world();
    set4f(p, "uView", ctx.viewC.x, ctx.viewC.y, ctx.viewH.x, ctx.viewH.y);
    set1f(p, "uPx", ctx.px);
    set1f(p, "uAspect", (float)ctx.width / ctx.height);
    set1f(p, "uWorld", world);
    set1f(p, "uPane", ctx.bare ? 0.f : 1.f - smoothstepf(0.f, 0.6f, saturate(ctx.bridge.amount)));
    set1f(p, "uDeepY", seaLevel());
    // the sun on the picture (its glare: none when it is behind the eye or hidden by the storm, faint for the moon and
    // under an overcast)
    const vec3 L = ctx.sunDir();
    const vec4 sp = ctx.vp * vec4(ctx.eye + L * 600.f, 1.f);
    const bool ahead = sp.w > 1e-3f && L.y > -0.02f;
    set2f(p, "uSunP", ahead ? sp.x / sp.w : 0.f, ahead ? sp.y / sp.w : 0.f);
    const float glare = ahead ? (1.f - ctx.storm) * lerpf(1.f, 0.3f, pal.moon) * (1.f - 0.75f * smoothstepf(0.4f, 0.8f, pal.clouds)) : 0.f;
    // the glass: smoky, darker than the world in every light (the variant's panel toward its deep sea), the world
    // half seen through it
    set4f(p, "uGlare", glare * (1.f - 0.6f * ctx.slow), 0.25f, 0.75f, 1.f);
    set1f(p, "uGrid", 0.24f);
    set3f(p, "uGlareCol", pal.sun * pal.lightK);
    set3f(p, "uFrostCol", lerp(pal.panel, pal.deep, 0.55f));
    set3f(p, "uPanel", pal.panel * 0.7f);
    ugl::drawFullscreen();
    glEnable(GL_DEPTH_TEST);
}

// How the world looks behind a point of the board plane (the HUD's ink): the sky above the horizon, the water's cut face
// under the sea's level, the sand's cut face, the sea's surface seen from above.
vec3 SeaWorld::behind(float bx, float by, float ndc) const {
    const float S = seaLevel();
    const Palette& pal = ctx.pal;
    vec3 c;
    if (by < sandH(bx, FRONT_Z)) c = lerp(vec3(0.62f, 0.47f, 0.29f), pal.haze, 0.15f) * (1.f - 0.62f * pal.night) * 0.7f;
    else if (by < S) c = lerp(pal.sea2, pal.deep * 0.2f, smoothstepf(0.5f, 10.f, S - by) * 0.92f);
    else if (ndc < ctx.horizon) c = lerp(pal.sea1, pal.skyLow, 0.5f);
    else {
        const float h = saturate((ndc - ctx.horizon) * 0.344f / 0.24f);
        c = lerp(pal.skyLow, pal.skyTop, smoothstepf(0.f, 1.f, std::pow(h, 0.7f)));
        c = lerp(c, pal.stormSky, ctx.storm * 0.85f);
    }
    if (by >= S) c = lerp(c, pal.stormSea, ctx.storm * 0.4f);
    return c;
}

// The stack's cubes: full of the sea's water, under pressure, always over the dark glass of the well and the panels
// (their water luminous, a bright reflection), whatever the light outside.
void SeaWorld::cellUniforms(GLuint p) const {
    set1f(p, "uBulge", 0.012f);
    set1f(p, "uTiltY", 0.27f);
    set1f(p, "uLightPanel", 0.f);
    set4f(p, "uSurf", 0.6f, 1.f, 1.1f, 0.6f);
}

} // namespace biome
