#pragma once
// BIOME's land: glass cubes of seeds breaking open over parched clay, a jungle growing (see land.cpp).
#include <cmath>
#include <vector>

#include "universes/biome/biome.hpp"

namespace biome {

namespace land {
// how many cleared lines turn the dry land into a full jungle (a Tetris counts 1.5 per line)
constexpr float FULL_LINES = 60.f;
// the seeds fly light, on high arcs
constexpr float SEED_GRAV = 15.f;
// the life map: a square of the land behind the well, one texel per LIFE_CELL board units
constexpr int LIFE_RES = 128;
constexpr float LIFE_X0 = -64.f, LIFE_Z0 = -1.f, LIFE_CELL = 1.f; // x from -64 to 64, z from -1 back to -129
// the camera's eye height (board units)
constexpr float EYE_Y = 5.2f;
// at most this many sown seeds shown sprouting near at once (the others are sown far out, or wait their turn)
constexpr int MAX_SPROUTS = 12;

// The land: a wide shallow plain behind the well, gently rolling, rising far out into a line of low hills on the
// horizon (z <= FRONT_Z). WORLD_GLSL's groundH is the same function.
inline float groundH(float x, float z) {
    const float dz = -z;
    const float d = 0.35f * std::sin(x * 0.23f + 1.3f + z * 0.13f) + 0.22f * std::sin(x * 0.61f + 0.4f - z * 0.29f) +
                    0.12f * std::sin(x * 1.3f + 2.f + z * 0.7f);
    const float hill = smoothstepf(30.f, 170.f, dz) *
                       (5.f + 4.f * std::sin(x * 0.035f + 0.8f) + 3.f * std::sin(x * 0.071f - z * 0.02f + 2.1f) +
                        2.f * std::sin(x * 0.13f + z * 0.05f));
    return -9.4f + 1.2f * smoothstepf(5.f, 60.f, dz) + d * (1.f + 0.8f * smoothstepf(10.f, 80.f, dz)) + hill;
}
inline vec3 groundN(float x, float z) {
    const float e = 0.05f;
    return normalize(vec3(groundH(x - e, z) - groundH(x + e, z), 2.f * e, groundH(x, z - e) - groundH(x, z + e)));
}

// A small value noise (only the simulation uses it, to cluster the forest).
inline float hash2(float x, float y) {
    const uint64_t h = splitmix64((uint64_t)(int64_t)std::floor(x) * 73856093ull ^ (uint64_t)(int64_t)std::floor(y) * 19349663ull);
    return (h >> 40) * (1.0f / 16777216.0f);
}
inline float vnoise2(float x, float y) {
    const float ix = std::floor(x), iy = std::floor(y), fx = x - ix, fy = y - iy;
    const float ux = fx * fx * (3.f - 2.f * fx), uy = fy * fy * (3.f - 2.f * fy);
    const float a = hash2(ix, iy), b = hash2(ix + 1.f, iy), c = hash2(ix, iy + 1.f), d = hash2(ix + 1.f, iy + 1.f);
    return lerpf(lerpf(a, b, ux), lerpf(c, d, ux), uy);
}

// the land's variants (their names: the registry lists them without GL)
int variantCount();
const char* variantName(int i);
} // namespace land

class LandWorld : public World {
public:
    explicit LandWorld(Ctx& ctx) : World(ctx) {}
    ~LandWorld() override;
    bool init() override;
    Palette palette(int i) const override;
    int pickVariant(const SongLook& look) const override;
    void setSong(const SongLook& look, float seconds, bool fresh) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void breakOut(const vec3& p, int type) override;
    void dropHit() override;
    void kick() override;
    void grown() override;
    void simulate(const Game& game, const MusicState& music, float dt, float wdt) override;
    float eyeY() const override { return land::EYE_Y; }
    float stormIn() const override { return 0.7f; }
    float cloudDrift() const override { return 0.03f; }
    void shadowPass() override;
    void drawWorld() override;
    void drawAfterStack() override;
    vec3 behind(float bx, float by, float ndc) const override;
    vec3 rainColor() const override { return lerp(ctx.pal.mist, vec3(1.f), 0.4f); }
    GLuint cellProgram() const override { return progCell_; }
    void cellUniforms(GLuint p) const override;
    bool seedGauge() const override { return true; }
    vec3 background(const Palette& p) const override { return lerp(p.skyLow, p.soilDry, 0.45f); }

    // The plants' kinds (their meshes); the last two are a sown seed's soil mound and its sprout.
    enum { K_GRASS = 0, K_FLOWER, K_FERN, K_BUSH, K_TREE, K_EMERGENT, K_PALM, K_DEAD, K_MOUND, K_SPROUT, K_COUNT };

private:
    // ---- Seeds flying out of a broken cube on their arc into the land (their trail behind them), dust,
    // rain splashes, pollen.
    enum { P_SEED = 0, P_DUST, P_SPLASH, P_MOTE, P_PETAL, P_TRAIL };
    struct Particle {
        vec3 p, v;
        float age, life, size, grav, drag, seed, spin;
        int kind, type;
        vec3 col;
        bool real;          // ages at the real speed, even in slow motion
        bool hero = false;  // a seed sown near, seen sprouting
        int bounces = 0;
        float trail = 0.f, pause = 0.f;
    };
    std::vector<Particle> parts_;
    // a cube breaks open: `seeds` seeds bursting out (`heroes` of them sown near, sprouting in view
    // after `pause` seconds and more)
    void breakCube(const vec3& p, int type, float power, float seeds, int heroes, float pause);
    void dustPuff(const vec3& p, int n, float power);
    void simulateParticles(float dt, float wdt);
    // A lock: a small puff of dust pushed out from under the piece (board plane, drawn after the stack).
    struct Puff { vec2 p, v; float age, life, size; };
    std::vector<Puff> puffs_;

    // ---- A seed come to rest on the soil: it settles, buries itself under a small mound of soil, and (near) a sprout
    // pushes out of the mound, its hook straightening, its two seed leaves unfolding; the plant then grows out of it.
    struct Sow {
        vec3 p, col;
        int type, kind;
        float t, pause, rot, seed, scale, size, spin;
        bool hero, planted;
    };
    std::vector<Sow> sows_;
    int heroes() const;        // the sprouts on show
    void settle(const Particle& s);
    void simulateSows(float dt);
    float sproutAt(const Sow& s) const { return s.t - (0.95f + s.pause); }   // the sprout's age (negative: buried)

    // ---- The plants: each planted seed grows (slowly: grass in seconds, trees over minutes), sways, blooms.
    struct Plant {
        vec3 p;
        int kind;
        float g, rate, scale, rot, seed, tint, vines, wither, cov;
        bool wild;   // grown on its own far out (the forest spreading), not from a seed
    };
    std::vector<Plant> plants_;
    int plantCount_[K_COUNT] = {};
    void plant(const vec3& p, int kind, float g0, bool wild, int type);
    void growPlants(float wdt);
    void spreadWild(float wdt, bool instant);
    int pickKind(float x, float z, int type);
    void seedInstant(float lines);   // a land already grown by `lines` cleared lines (a stack joined in progress)
    void plantStumps();              // the dead trees of the parched land
    void resetLand();

    // ---- The life map: grass cover, damp soil and flowers around what grows (a texture over the land).
    std::vector<float> lifeG_, lifeM_, lifeF_;
    std::vector<unsigned char> lifeTex_;
    bool lifeDirty_ = true;
    float lifeUpload_ = 0.f, lastFrame_ = 0.f;
    void stamp(float x, float z, float radius, float g, float m, float f);
    void uploadLife(float dt);

    // ---- Progress: how alive the land is (the lines cleared this song), the wet soil.
    float lines_ = 0.f;       // cleared lines (Tetris lines count 1.5)
    float life_ = 0.f;        // 0 parched .. 1 jungle, shown (approaches the target)
    float lifeTarget() const { return std::min(1.f, lines_ / land::FULL_LINES); }
    float wet_ = 0.f;
    float surge_ = 0.f;       // the drop's growth surge (decays slowly: what it grew stays)
    float bloom_ = 0.f;       // flowers open (peaks, drops, Tetris)
    float wind_ = 0.1f, swayT_ = 0.f;
    float wildAcc_ = 0.f;
    bool started_ = false;

    // ---- Birds (flying around and between the trees), butterflies, fireflies.
    struct Bird {
        vec3 c;              // the center it circles
        float r, ang, w, h, phase, size, dy, alpha, life, flap;
        int kind;            // 0 bird, 1 colorful bird, 2 butterfly, 3 firefly
        vec3 col, p, v;
    };
    std::vector<Bird> birds_;
    float birdT_ = 0.f;
    void spawnBird(int kind, bool onScreen, bool flock);
    void simulateBirds(float dt, float wdt, float world);

    // ---- GL.
    struct Mesh { GLuint vao = 0, vbo = 0, ibo = 0; int count = 0; };
    void buildMeshes();
    void worldUniforms(GLuint prog);
    void plantUniforms(GLuint prog);
    void drawPlants(bool shadow);
    GLuint progSky_ = 0, progGround_ = 0, progPlant_ = 0, progDepth_ = 0, progBird_ = 0, progPart_ = 0, progCell_ = 0;
    Mesh ground_;
    // the plants' meshes: one per kind, near (detailed) and far (simple)
    GLuint plantVao_ = 0, plantVbo_ = 0, plantInstVbo_ = 0;
    int meshFirst_[K_COUNT][2] = {}, meshCount_[K_COUNT][2] = {};
    GLuint birdVao_ = 0, birdVbo_ = 0;
    GLuint lifeTexId_ = 0;
};

} // namespace biome
