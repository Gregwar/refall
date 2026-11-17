// BIOME's land, its drawing (see land.cpp): the GL resources, the world's meshes (the land and its cut face, the
// plants, the sown seeds' mounds and sprouts), its passes.
//
// The shadow map: the plants near the well. The world, multisampled, with depth: the sky, the land, the plants (and
// the mounds and sprouts), the birds, the seeds in flight and at rest, the dust, the splashes and the pollen, the
// sprouts' glow; the light shafts through the air (marched up to the world's depth); the shell's lens (the bloom, the
// glare, the well's smoky glass). The shell then adds the rain; over the glass, behind the stack, the falling piece's
// warm glow and the green sparks of life; then the stack; the puffs of dust under a lock come after the stack.
#include <algorithm>
#include <cstdio>

#include "layout.hpp"
#include "universes/biome/gl.hpp"
#include "universes/biome/land.hpp"
#include "universes/biome/land_shaders.hpp"
#include "universes/biome/shaders.hpp"

namespace biome {

using namespace land;
namespace shaders = land_shaders;
namespace common = biome_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

namespace {

// Low-poly plant meshes: position, normal, anchor (a leaf clump's or a flower's center, a vine's top), material
// (0 bark, 1 leaves, 2 lighter leaves, 3 grass, 4 petals, 5 a flower's heart, 6 vines, 7 dead wood, 8 fronds, 9 soil,
// 10 a sprout's stem, 11 its seed leaves, 12 broad leaves), sway
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
    // a fern's frond from `a` to `tip` arching: a thin stalk, its leaflets in pairs along it, smaller toward the tip
    void frond(const vec3& a, const vec3& tip, float rise, int pairs, int m) {
        blade(a, tip, rise, 0.025f, m, vec3(0.f), 4);
        const vec3 side = normalize(cross(tip - a, vec3(0, 1, 0)));
        auto at = [&](float t) { return a + (tip - a) * t + vec3(0.f, rise * 4.f * t * (1.f - t), 0.f); };
        for (int i = 1; i <= pairs; i++) {
            const float t = (float)i / (pairs + 1);
            const vec3 p = at(t), fwd = normalize(at(t + 0.04f) - p);
            const float len = 0.24f * (1.f - t) + 0.05f;
            for (int sd = -1; sd <= 1; sd += 2) {
                const vec3 out = p + side * (len * (float)sd) + fwd * (len * 0.35f) - vec3(0.f, len * 0.3f, 0.f);
                tri(p - fwd * 0.03f, out, p + fwd * 0.07f, vec3(0.f), m);
            }
        }
    }
    // a broad leaf from `a` along `dir` (horizontal, unit), `len` long: rising, then drooping at its tip, widest past
    // its middle, folded a little along its midrib
    void leaf(const vec3& a, const vec3& dir, float len, float width, float rise, int segs, int m) {
        const vec3 side = normalize(cross(dir, vec3(0, 1, 0)));
        auto at = [&](float t) { return a + dir * (len * t) + vec3(0.f, rise * std::sin(t * 2.4f) - len * 0.35f * t * t, 0.f); };
        auto half = [&](float t) { return width * std::pow(std::sin(PI * std::min(1.f, t * 1.05f)), 0.8f) * (1.f - 0.15f * t); };
        for (int i = 0; i < segs; i++) {
            const float t0 = (float)i / segs, t1 = (float)(i + 1) / segs;
            const vec3 m0 = at(t0), m1 = at(t1);
            const vec3 lift0(0.f, half(t0) * 0.25f, 0.f), lift1(0.f, half(t1) * 0.25f, 0.f);
            quad(m0 - side * half(t0), m0 + lift0, m1 + lift1, m1 - side * half(t1), a, m);
            quad(m0 + lift0, m0 + side * half(t0), m1 + side * half(t1), m1 + lift1, a, m);
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

bool LandWorld::init() {
    const char* W = common::SKY_GLSL;
    const char* S = common::SHADOW_GLSL;
    progSky_ = compile(ugl::FULLSCREEN_VS, shaders::SKY_FS, "land sky", {W, shaders::WORLD_GLSL});
    progGround_ = compile(shaders::GROUND_VS, shaders::GROUND_FS, "land ground", {W, shaders::WORLD_GLSL, S});
    progPlant_ = compile(shaders::PLANT_VS, shaders::PLANT_FS, "land plant", {W, shaders::WORLD_GLSL, S});
    progDepth_ = compile(shaders::PLANT_VS, common::DEPTH_FS, "land depth", {});
    progBird_ = compile(shaders::BIRD_VS, shaders::BIRD_FS, "land bird", {W, shaders::WORLD_GLSL});
    progPart_ = compile(common::PART_VS, common::PART_FS, "land particle", {W, shaders::WORLD_GLSL});
    progShaft_ = compile(ugl::FULLSCREEN_VS, shaders::SHAFT_FS, "land shafts", {W});
    progCell_ = compile(common::CELL_VS, std::string(common::CELL_HEAD) + shaders::CELL_FILL + common::CELL_MAIN, "land cube",
                        {common::GLASS_GLSL});
    ctx.instanced(birdVao_, birdVbo_, 4);
    buildMeshes();

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
    return glGetError() == GL_NO_ERROR;
}

// The world's meshes: the land (a grid denser near the well, wider far out, its cut face along the front) and the
// plants (each kind near and far: a simpler one for the distant forest).
void LandWorld::buildMeshes() {
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
            const vec3 tip = d * b.r.range(0.9f, 1.3f) + vec3(0.f, 0.15f, 0.f);
            if (nearM) b.frond(vec3(0.f), tip, 0.75f, 7, 8);
            else b.blade(vec3(0.f), tip, 0.75f, 0.17f, 8, vec3(0.f), 4);
        }
        done(K_FERN, lod);
        // a broad-leaved plant of the undergrowth: big leaves on short stalks, rising and drooping all around
        mark(K_BROADLEAF, lod);
        b.swayH = 1.4f; b.swayK = 0.45f;
        {
            const int n = nearM ? 7 : 4;
            for (int i = 0; i < n; i++) {
                const float a = TAU * i / n + b.r.range(-0.25f, 0.25f);
                const vec3 d(std::cos(a), 0.f, std::sin(a));
                const float h = b.r.range(0.25f, 0.6f);
                const vec3 base = d * 0.12f + vec3(0.f, h, 0.f);
                b.blade(vec3(0.f), base, 0.f, 0.03f, 3, vec3(0.f), 1);
                b.leaf(base, d, b.r.range(0.85f, 1.25f), b.r.range(0.26f, 0.36f), 0.35f, nearM ? 5 : 3, 12);
            }
        }
        done(K_BROADLEAF, lod);
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
        // a sown seed's mound: a low lumpy dome of turned soil (PLANT_VS heaps it up and lets it settle)
        mark(K_MOUND, lod);
        b.swayH = 1.f; b.swayK = 0.f;
        {
            const int S = nearM ? 12 : 7, R = 3;
            auto at = [&](int ring, int i) {
                const float ph = (float)ring / R * PI * 0.5f, a = TAU * (float)i / S;
                const float lump = 1.f + 0.13f * std::sin(a * 3.f + ring * 1.7f) + 0.06f * std::sin(a * 7.f + 1.f);
                return vec3(std::cos(a) * std::cos(ph) * 0.36f * lump, std::sin(ph) * 0.16f - 0.02f, std::sin(a) * std::cos(ph) * 0.36f * lump);
            };
            for (int r = 0; r < R; r++)
                for (int i = 0; i < S; i++) {
                    if (r == R - 1) b.tri(at(r, i), at(R, 0), at(r, i + 1), vec3(0.f), 9);
                    else b.quad(at(r, i), at(r + 1, i), at(r + 1, i + 1), at(r, i + 1), vec3(0.f), 9);
                }
        }
        done(K_MOUND, lod);
        // a sprout: a stem (in segments: PLANT_VS bends it into a hook) and two seed leaves at its top
        mark(K_SPROUT, lod);
        b.swayH = 0.6f; b.swayK = 0.25f;
        {
            const float HS = 0.55f;
            const int segs = nearM ? 6 : 3;
            for (int i = 0; i < segs; i++)
                b.cyl(vec3(0.f, HS * i / segs, 0.f), vec3(0.f, HS * (i + 1) / segs, 0.f), 0.042f - 0.01f * i / segs,
                      0.042f - 0.01f * (i + 1) / segs, 5, 10);
            const vec3 hc(0.f, HS, 0.f);
            const float T[4] = {0.f, 0.35f, 0.72f, 1.f}, Wd[4] = {0.016f, 0.11f, 0.1f, 0.f};
            for (int s = -1; s <= 1; s += 2)
                for (int i = 0; i < 3; i++) {
                    const vec3 a = hc + vec3(s * 0.3f * T[i], 0.03f * T[i], 0.f), c = hc + vec3(s * 0.3f * T[i + 1], 0.03f * T[i + 1], 0.f);
                    b.quad(a - vec3(0.f, 0.f, Wd[i]), a + vec3(0.f, 0.f, Wd[i]), c + vec3(0.f, 0.f, Wd[i + 1]),
                           c - vec3(0.f, 0.f, Wd[i + 1]), hc, 11);
                }
        }
        done(K_SPROUT, lod);
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

LandWorld::~LandWorld() {
    for (GLuint p : {progSky_, progGround_, progPlant_, progDepth_, progBird_, progPart_, progCell_, progShaft_}) glDeleteProgram(p);
    glDeleteFramebuffers(1, &depthFbo_);
    glDeleteTextures(1, &depthTex_);
    glDeleteVertexArrays(1, &ground_.vao);
    glDeleteBuffers(1, &ground_.vbo);
    glDeleteBuffers(1, &ground_.ibo);
    for (GLuint v : {plantVao_, birdVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {plantVbo_, plantInstVbo_, birdVbo_}) glDeleteBuffers(1, &b);
    glDeleteTextures(1, &lifeTexId_);
}

// =====================================================================================================
// Uniforms, plants
// =====================================================================================================

void LandWorld::worldUniforms(GLuint p) {
    // the light: the sun's, a steady glow per section (brighter at a peak), dimmed by the rain's overcast; a few high
    // wisps over the dry land, more clouds over the living one, an overcast in the rain; the dust veiling the dry sky
    const float calm = 1.f - smoothstepf(0.f, 1.f, ctx.lv);
    const float glow = (1.f + 0.12f * ctx.wp() - 0.1f * calm) * (1.f - 0.7f * ctx.storm) * (0.9f + 0.1f * life_);
    ctx.skyUniforms(p, glow, ctx.pal.clouds * (0.5f + 0.7f * life_) + 0.7f * ctx.storm, v2(0.03f, 0.16f), 0.3f,
                    (1.f - smoothstepf(0.f, 0.7f, life_)) * 0.4f);
    set1f(p, "uLife", life_);
    set1f(p, "uWet", wet_);
    set3f(p, "uMist", ctx.pal.mist);
    set3f(p, "uLifeArea", vec3(LIFE_X0, LIFE_Z0, LIFE_RES * LIFE_CELL));
    set1i(p, "uLifeMap", 6);
}

void LandWorld::plantUniforms(GLuint p) {
    set1f(p, "uSwayT", swayT_);
    set1f(p, "uWind", wind_);
    set1f(p, "uGust", ctx.gust * ctx.world());
    set1f(p, "uBloom", bloom_);
    setMat(p, "uVP", ctx.vp);
}

// The plants, one instanced draw per kind and detail (the far forest simpler), the sown seeds' mounds and sprouts with
// them; `shadow`: into the shadow map (the near ones only).
void LandWorld::drawPlants(bool shadow) {
    if (plants_.empty() && sows_.empty()) return;
    glBindVertexArray(plantVao_);
    glBindBuffer(GL_ARRAY_BUFFER, plantInstVbo_);
    static std::vector<float> inst[K_COUNT][2];
    for (int k = 0; k < K_COUNT; k++)
        for (int l = 0; l < 2; l++) inst[k][l].clear();
    for (const Plant& p : plants_) {
        if (p.g <= 0.f) continue;
        if (shadow && p.p.z < -60.f) continue;
        const float reach = 6.f * p.scale + 2.f;
        if (std::fabs(p.p.x) > ctx.hwAt(p.p.z) + reach) continue;
        const int lod = p.p.z < -45.f ? 1 : 0;
        // a young plant of the undergrowth sown near glows as it comes up
        const bool low = p.kind <= K_BUSH || p.kind == K_BROADLEAF;
        const float glow = !shadow && !p.wild && low && p.p.z > -16.f && p.wither <= 0.f ? 1.f - smoothstepf(0.f, 0.7f, p.g) : 0.f;
        const float v[12] = {p.p.x, p.p.y, p.p.z, p.scale, p.rot, p.g, p.seed, p.vines,
                             p.tint, p.wither > 0.f ? std::min(1.f, p.wither * 0.6f) : 0.f, (float)p.kind, glow};
        inst[p.kind][lod].insert(inst[p.kind][lod].end(), v, v + 12);
    }
    for (const Sow& s : sows_) {
        if (!s.hero) continue;
        const float a = sproutAt(s);
        const float gone = smoothstepf(4.5f, 7.f, a);
        // the mound: heaped up as the seed sinks, settling a little as the sprout pushes through, then flat
        const float mound = smoothstepf(0.35f, 0.95f, s.t) * (1.f - 0.3f * smoothstepf(0.f, 1.f, a)) * (1.f - gone);
        if (mound > 0.001f) {
            const float v[12] = {s.p.x, s.p.y, s.p.z, s.scale, s.rot, mound, s.seed, 0.f, 0.f, 0.f, (float)K_MOUND, 0.f};
            inst[K_MOUND][0].insert(inst[K_MOUND][0].end(), v, v + 12);
        }
        // the sprout: out of the mound and opening over 1.8 s, then its seed leaves wither as the plant takes over
        if (a > 0.f && gone < 1.f) {
            const float g = saturate(a / 1.8f);
            const float v[12] = {s.p.x, s.p.y + 0.03f, s.p.z, s.scale * 2.f * (1.f - gone), s.rot, g, s.seed, 0.f, 0.f, 0.f, (float)K_SPROUT, 1.f};
            inst[K_SPROUT][0].insert(inst[K_SPROUT][0].end(), v, v + 12);
        }
    }
    for (int k = 0; k < K_COUNT; k++)
        for (int l = 0; l < 2; l++) {
            const std::vector<float>& in = inst[k][l];
            if (in.empty()) continue;
            glBufferData(GL_ARRAY_BUFFER, in.size() * sizeof(float), in.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLES, meshFirst_[k][l], meshCount_[k][l], (GLsizei)(in.size() / 12));
        }
}

// The life map, uploaded when it changed (a few times a second at most).
void LandWorld::uploadLife(float dt) {
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
}

// =====================================================================================================
// Passes
// =====================================================================================================

void LandWorld::shadowPass() {
    const vec3 L = ctx.sunDir();
    const vec3 c(0.f, -6.f, -26.f);
    const vec3 up = std::fabs(L.y) > 0.95f ? vec3(0, 0, 1) : vec3(0, 1, 0);
    ctx.lightVP = mat4::ortho(-48.f, 48.f, -40.f, 40.f, 1.f, 300.f) * mat4::lookAt(c + L * 140.f, c, up);
    glUseProgram(progDepth_);
    plantUniforms(progDepth_);
    setMat(progDepth_, "uVP", ctx.lightVP);
    drawPlants(true);
}

void LandWorld::drawWorld() {
    const float world = ctx.world();
    uploadLife(ctx.time - lastFrame_);
    lastFrame_ = ctx.time;
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, lifeTexId_);
    glActiveTexture(GL_TEXTURE0);
    auto shadowUniforms = [this](GLuint p) {
        set1i(p, "uShadow", 5);
        setMat(p, "uLightVP", ctx.lightVP);
        set1f(p, "uShadowK", ctx.pal.shadowK * (1.f - 0.75f * ctx.storm));
    };
    // the sky
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glUseProgram(progSky_);
    worldUniforms(progSky_);
    setMat(progSky_, "uInvVP", ctx.invVP);
    ugl::drawFullscreen();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    // the land
    glUseProgram(progGround_);
    worldUniforms(progGround_);
    shadowUniforms(progGround_);
    const Palette& pal = ctx.pal;
    set3f(progGround_, "uSoilDry", pal.soilDry);
    set3f(progGround_, "uSoilWet", pal.soilWet);
    set3f(progGround_, "uSoilCut", pal.soilCut);
    set3f(progGround_, "uCrack", pal.crack);
    set3f(progGround_, "uGrassDry", pal.grassDry);
    set3f(progGround_, "uGrassLush", pal.grassLush);
    set3f(progGround_, "uLeafFar", pal.leafFar);
    set3f(progGround_, "uFlowerA", pal.flowerA);
    set3f(progGround_, "uFlowerB", pal.flowerB);
    set1f(progGround_, "uPx", ctx.px);
    set1f(progGround_, "uBloom", bloom_);
    glBindVertexArray(ground_.vao);
    glDrawElements(GL_TRIANGLES, ground_.count, GL_UNSIGNED_INT, nullptr);
    // the plants, the mounds and the sprouts
    if (!plants_.empty() || !sows_.empty()) {
        glUseProgram(progPlant_);
        worldUniforms(progPlant_);
        shadowUniforms(progPlant_);
        plantUniforms(progPlant_);
        set3f(progPlant_, "uBark", pal.bark);
        set3f(progPlant_, "uLeafA", pal.leafA);
        set3f(progPlant_, "uLeafB", pal.leafB);
        set3f(progPlant_, "uGrassDry", pal.grassDry);
        set3f(progPlant_, "uGrassLush", pal.grassLush);
        set3f(progPlant_, "uDead", pal.dead);
        set3f(progPlant_, "uFlowerC", lerp(pal.flowerB, vec3(1.f, 0.85f, 0.3f), 0.5f));
        set3f(progPlant_, "uLeafFar", pal.leafFar);
        set3f(progPlant_, "uGlow", glowColor() * ctx.world());
        // the turned soil: darker and damper than the dry crust around it
        set3f(progPlant_, "uSoil", lerp(lerp(pal.soilDry, pal.soilWet, 0.62f), pal.soilWet, wet_ * 0.6f));
        float pc[21];
        for (int i = 0; i < 7; i++) {
            const vec3 c = lerp(pal.piece[i], pal.flowerA, 0.15f);
            pc[i * 3] = c.x; pc[i * 3 + 1] = c.y; pc[i * 3 + 2] = c.z;
        }
        glUniform3fv(ugl::U(progPlant_, "uPieces"), 7, pc);
        drawPlants(false);
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
            set3f(progBird_, "uCamR", ctx.camR);
            set3f(progBird_, "uCamU", ctx.camU);
            set1f(progBird_, "uPx", ctx.px);
            glBindVertexArray(birdVao_);
            glBindBuffer(GL_ARRAY_BUFFER, birdVbo_);
            glBufferData(GL_ARRAY_BUFFER, inst.size() * sizeof(float), inst.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(inst.size() / 16));
        }
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // the particles, all in the world (hidden by what stands in front of them; the ones still in the well seen through
    // its pane, never over the stack): the seeds in flight and their trails, the dust, the splashes, the
    // pollen; the seeds at rest on the soil, sinking into it
    std::vector<float> v;
    auto put = [&v, world](const vec3& p, const vec3& vel, float size, int kind, const vec3& col, float alpha, float stretch,
                           float soft, float seed, float ang) {
        if (alpha * world <= 0.003f) return;
        const float q[16] = {p.x, p.y, p.z, size, vel.x, vel.y, vel.z, (float)kind, col.x, col.y, col.z, alpha * world, stretch, soft, seed, ang};
        v.insert(v.end(), q, q + 16);
    };
    for (const Particle& p : parts_) {
        if (p.age < 0.f) continue;
        const float t = p.age / p.life;
        const float fade = 1.f - smoothstepf(0.6f, 1.f, t);
        const float spin = p.spin * p.age + p.seed * 6.f;
        switch (p.kind) {
        case P_SEED: put(p.p, p.v, p.size, 1, p.col, 1.f, 0.f, 0.f, p.seed, spin); break;
        case P_TRAIL: put(p.p, vec3(0.f), p.size * (1.f - 0.5f * t), 0, p.col, 0.6f * (1.f - t) * (1.f - t), 0.f, 0.85f, p.seed, 0.f); break;
        case P_DUST: put(p.p, p.v, p.size * (1.f + 1.5f * t), 5, p.col, 0.5f * (1.f - t), 0.f, 0.f, p.seed, 0.f); break;
        case P_SPLASH: put(p.p, p.v, p.size, 0, p.col, 0.7f * fade, 0.25f, 0.5f, p.seed, 0.f); break;
        case P_MOTE: put(p.p, p.v, p.size, 0, p.col, 0.55f * smoothstepf(0.f, 0.15f, t) * fade, 0.f, 0.8f, p.seed, 0.f); break;
        case P_PETAL: put(p.p, p.v, p.size, 1, p.col, fade, 0.f, 0.f, p.seed, spin); break;
        default: break;
        }
    }
    for (const Sow& s : sows_) {
        // lying on the soil, then sinking into it (the ground hides it)
        const float sink = smoothstepf(0.35f, 0.95f, s.t);
        if (sink >= 1.f) continue;
        put(s.p + vec3(0.f, s.size * (0.45f - 1.1f * sink), 0.f), vec3(0.f), s.size, 1, s.col, 1.f, 0.f, 0.f, s.seed, s.spin);
    }
    if (!v.empty()) {
        glUseProgram(progPart_);
        worldUniforms(progPart_);
        set1f(progPart_, "uAerial", 1.f);
        ctx.drawParts(progPart_, v);
    }
    // a soft green light around each sprout coming up near (added: the lens blooms it)
    v.clear();
    const vec3 glow = glowColor();
    for (const Sow& s : sows_) {
        if (!s.hero) continue;
        const float a = sproutAt(s);
        const float k = smoothstepf(-0.3f, 0.5f, a) * (1.f - smoothstepf(3.f, 6.5f, a));
        put(s.p + vec3(0.f, 0.4f, 0.f), vec3(0.f), 1.6f, 5, glow, 0.4f * k, 0.f, 0.f, s.seed, 0.f);
    }
    if (!v.empty()) {
        glUseProgram(progPart_);
        worldUniforms(progPart_);
        set1f(progPart_, "uAerial", 1.f);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        ctx.drawParts(progPart_, v);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glDepthMask(GL_TRUE);
    drawShafts();
    // the lens: the bloom, the sun's glare, the well's dark smoky glass (the world dim behind the stack in every light)
    Ctx::Lens l;
    l.glare = (1.f - ctx.storm) * lerpf(1.f, 0.3f, pal.moon) * (1.f - 0.75f * smoothstepf(0.4f, 0.8f, pal.clouds)) * (1.f - 0.6f * ctx.slow);
    l.bloom = 0.3f;
    l.grid = 0.14f;
    l.panels = false;
    l.glareCol = pal.sun * pal.lightK;
    l.frostCol = lerp(pal.panel, pal.leafFar * 0.3f, 0.3f);
    l.panel = pal.panel * 0.7f;
    ctx.drawLens(l);
}

// The light shafts (SHAFT_FS) through the canopy into the mist, added over the world: stronger as the forest closes
// over the land (its gaps), dimmed by the rain's overcast but never gone (the light breaking through).
void LandWorld::drawShafts() {
    const Palette& pal = ctx.pal;
    const float canopy = smoothstepf(0.12f, 0.7f, life_);
    const float k = (0.25f + 0.75f * canopy) * (1.f - 0.45f * ctx.storm) * ctx.world();
    if (k <= 0.01f) return;
    if (depthW_ != ctx.width || depthH_ != ctx.height) {
        depthW_ = ctx.width;
        depthH_ = ctx.height;
        if (!depthTex_) glGenTextures(1, &depthTex_);
        glBindTexture(GL_TEXTURE_2D, depthTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, depthW_, depthH_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (!depthFbo_) glGenFramebuffers(1, &depthFbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, depthFbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex_, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) std::fprintf(stderr, "[gl] land depth FBO incomplete\n");
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx.msFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, depthFbo_);
    glBlitFramebuffer(0, 0, ctx.width, ctx.height, 0, 0, ctx.width, ctx.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx.msFbo);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    const GLuint p = progShaft_;
    glUseProgram(p);
    set3f(p, "uEye", ctx.eye);
    set1f(p, "uFrontZ", FRONT_Z);
    set1f(p, "uTime", ctx.time);
    setMat(p, "uInvVP", ctx.invVP);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, depthTex_);
    glActiveTexture(GL_TEXTURE0);
    set1i(p, "uDepth", 7);
    const vec3 L = ctx.sunDir();
    set3f(p, "uShaftDir", normalize(vec3(L.x < 0.f ? -0.55f : 0.55f, 1.f, -0.3f)));
    set3f(p, "uShaftCol", lerp(pal.sun, pal.mist, 0.3f) * (pal.lightK * k * 1.f));
    set1f(p, "uGaps", 1.f - canopy);
    ugl::drawFullscreen();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// Over the well's glass, behind the stack: the falling piece's warm glow on the glass, and the green sparks of life
// (out of a cleared row, rising around the sprouts), added, behind what stands in front of them in the world.
void LandWorld::drawAfterPane() {
    if (ctx.bare) return;
    const float world = ctx.world();
    std::vector<float> v;
    auto put = [&v, world](const vec3& p, const vec3& vel, float size, int kind, const vec3& col, float alpha, float stretch, float soft) {
        if (alpha * world <= 0.003f) return;
        const float q[16] = {p.x, p.y, p.z, size, vel.x, vel.y, vel.z, (float)kind, col.x, col.y, col.z, alpha * world, stretch, soft, 0.f, 0.f};
        v.insert(v.end(), q, q + 16);
    };
    const vec3 warm = lerp(ctx.pal.piece[fallType_], vec3(1.f, 0.6f, 0.25f), 0.5f);
    for (int i = 0; i < fallN_; i++) put(vec3(fall_[i].x, fall_[i].y, -0.05f), vec3(0.f), 1.3f, 5, warm, 0.2f, 0.f, 0.f);
    for (const Particle& p : parts_) {
        if (p.kind != P_SPARK || p.age < 0.f) continue;
        const float t = p.age / p.life;
        const float a = smoothstepf(0.f, 0.1f, t) * (1.f - smoothstepf(0.5f, 1.f, t)) * (0.75f + 0.25f * std::sin(p.age * 9.f + p.seed * 40.f));
        put(p.p, p.v, p.size, 0, p.col, a, 0.25f, 0.9f);
        put(p.p, vec3(0.f), p.size * 4.f, 5, p.col, a * 0.18f, 0.f, 0.f);
    }
    if (v.empty()) return;
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glUseProgram(progPart_);
    worldUniforms(progPart_);
    set1f(progPart_, "uAerial", 1.f);
    ctx.drawParts(progPart_, v);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
}

// The puffs of dust under a lock (at the contact, small), over the stack.
void LandWorld::drawAfterStack() {
    if (puffs_.empty() || ctx.bare) return;
    std::vector<float> pv;
    const vec3 dc = lerp(ctx.pal.soilDry, ctx.pal.dust, 0.5f);
    const float world = ctx.world();
    for (const Puff& q : puffs_) {
        const float t = q.age / q.life;
        const float v[16] = {q.p.x, q.p.y, 0.f, q.size * (1.f + 1.2f * t), q.v.x, q.v.y, 0.f, 5.f, dc.x, dc.y, dc.z,
                             0.5f * (1.f - t) * world, 0.f, 0.f, 0.f, 0.f};
        pv.insert(pv.end(), v, v + 16);
    }
    glUseProgram(progPart_);
    worldUniforms(progPart_);
    set1f(progPart_, "uAerial", 0.f);
    ctx.drawParts(progPart_, pv);
}

// The world's color behind a point of the board plane: the sky above the horizon, the land below it (its soil or its
// green), the cut face under it.
vec3 LandWorld::behind(float bx, float by, float ndc) const {
    const Palette& pal = ctx.pal;
    vec3 c;
    if (by < groundH(bx, FRONT_Z)) c = pal.soilCut * 0.7f;
    else if (ndc < ctx.horizon) {
        const vec3 green = lerp(pal.grassLush, pal.leafA, 0.6f);
        c = lerp(lerp(pal.soilDry, pal.soilWet, wet_ * 0.7f), green, life_ * 0.85f);
        c = lerp(c, lerp(pal.dust, pal.mist, life_), 0.25f + 0.4f * smoothstepf(ctx.horizon - 0.3f, ctx.horizon, ndc));
    } else {
        const float h = saturate((ndc - ctx.horizon) * 0.344f / 0.3f);
        c = lerp(pal.skyLow, pal.skyTop, smoothstepf(0.f, 1.f, std::pow(h, 0.7f)));
        c = lerp(c, pal.stormSky, ctx.storm * 0.8f);
    }
    return c;
}

// The seeds' cubes: no bulge, seen a little more from above; a glossy reflection, a sharp highlight, bright edges.
void LandWorld::cellUniforms(GLuint p) const {
    set1f(p, "uBulge", 0.f);
    set1f(p, "uTiltY", 0.3f);
    set4f(p, "uSurf", 0.35f, 0.9f, 1.8f, 0.8f);
}

} // namespace biome
