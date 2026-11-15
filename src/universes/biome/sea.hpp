#pragma once
// BIOME's sea: glass cubes of water thrown out onto dry sand, the sea rising (see sea.cpp).
#include <cmath>
#include <vector>

#include "universes/biome/biome.hpp"

namespace biome {

namespace sea {
constexpr float WELL_X = 5.1f;
// the sea's level: it starts under the lowest sand (all dry, dunes out to the horizon), and rises as water falls in:
// first a lake in the hollow behind the well, then it spills over the far plain out to the horizon and creeps up the
// beach, up to about three quarters of the well's height
constexpr float SEA_LOW = -14.6f, SEA_HIGH = 5.0f;
// how many poured lines fill the sea: it rises linearly, every line by the same amount (a quad pours 1.5 per line)
constexpr float SEA_FULL_LINES = 60.f;
// the first boats sail out once the sea is this high
constexpr float BOAT_LEVEL = -10.4f;
// a submarine crosses now and then once the sea is this deep at the cut face (it is about 14 when full)
constexpr float SUB_DEPTH = 11.5f;
// gravity for the thrown cubes and the water (snappy)
constexpr float GRAV = 22.f;
// clear water, almost colorless (linear)
const vec3 WATER{0.74f, 0.86f, 0.95f};
// the boats' lengths (their meshes, scale 1)
constexpr float BOAT_LEN[6] = {1.6f, 3.2f, 4.6f, 10.f, 18.f, 22.f};

// the far plain rising gently out to the horizon (evenly with the log of the distance, as the eye sees it): the
// rising sea spreads out from the lake to the horizon instead of flooding the whole plain at once
constexpr float FAR_RISE = 5.f;
inline float farT(float z) { return saturate(std::log(std::max(-z, 30.f) / 30.f) / std::log(800.f / 30.f)); }
// the sandy bed: a beach under the well sloping gently down far out, a lake's hollow just behind the well (the lowest
// sand: the water gathers there first), low dunes over it (z <= FRONT_Z); WORLD_GLSL's sandH is the same function
inline float sandH(float x, float z) {
    const float lake = std::exp(-(x * x / 576.f + (z + 15.f) * (z + 15.f) / 100.f));
    const float base = -9.2f - 1.8f * smoothstepf(3.f, 48.f, -z) - 5.2f * lake + FAR_RISE * farT(z);
    const float k = (1.f - 0.4f * smoothstepf(10.f, 60.f, -z)) * (1.f - 0.8f * lake);
    const float d = 0.55f * std::sin(x * 0.21f + 1.3f + z * 0.11f) + 0.3f * std::sin(x * 0.53f + 0.4f - z * 0.27f) +
                    0.1f * std::sin(x * 1.37f + 2.f + z * 0.61f) + 0.35f * std::sin(x * 0.07f - z * 0.19f + 0.5f);
    return base + d * k;
}
inline vec3 sandN(float x, float z) {
    const float e = 0.05f;
    return normalize(vec3(sandH(x - e, z) - sandH(x + e, z), 2.f * e, sandH(x, z - e) - sandH(x, z + e)));
}
inline float crest(float s, float sharp) {
    const float c = s * 0.5f + 0.5f;
    return 2.f * std::pow(std::max(c, 0.f), 1.f + 2.5f * sharp) - 1.f;
}
// the sea's variants (their names: the registry lists them without GL)
int variantCount();
const char* variantName(int i);

inline float calmAt(float x, float z) {
    return 1.f - 0.75f * (1.f - smoothstepf(4.5f, 6.5f, std::fabs(x))) * (1.f - smoothstepf(4.f, 10.f, -z));
}
} // namespace sea

class SeaWorld : public World {
public:
    explicit SeaWorld(Ctx& ctx) : World(ctx) {}
    ~SeaWorld() override;
    bool init() override;
    Palette palette(int i) const override;
    int pickVariant(const SongLook& look) const override;
    void setSong(const SongLook& look, float seconds, bool fresh) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void breakOut(const vec3& p, int type) override;
    void dropHit() override;
    void kick() override;
    void grown() override;
    void simulate(const Game& game, const MusicState& music, float dt, float wdt) override;
    float eyeY() const override { return lerpf(4.5f, 8.f, rise_); }
    float stormIn() const override { return 1.f; }
    void shadowPass() override;
    void drawWorld() override;
    vec3 behind(float bx, float by, float ndc) const override;
    float paneDeepY() const override { return seaLevel(); }
    float rainFloorY() const override { return seaLevel(); }
    vec3 rainColor() const override { return lerp(ctx.pal.foam, ctx.pal.skyLow, 0.5f); }
    float rainSlant() const override { return dir_; }
    GLuint cellProgram() const override { return progCell_; }
    void cellUniforms(GLuint p) const override;
    vec3 background(const Palette& p) const override { return lerp(p.skyLow, p.sea1, 0.4f); }

private:
    // ---- Cubes thrown out of a cleared row: rigid bodies tumbling on all axes into the world behind the well; each
    // cracks where it first hits (the sand, the sea, another cube) and its water jets out of the crack under pressure,
    // raising the sea; empty, it sinks into the sand.
    struct Body {
        vec3 p, v, w;           // position, velocity, angular velocity (world)
        float q[4];             // orientation (x, y, z, w)
        vec3 crackL, crackN;    // the crack: where on the glass and its outward normal (the cube's frame)
        int type;
        float seed, age, delay, hitT, crack, fill, water, pend, acc, fade, still;
        bool hit, wet, asleep, passed;
    };
    std::vector<Body> bodies_;
    void throwCube(const vec3& p, int type, const vec3& v, const vec3& w, float delay, float water);
    void simulateBodies(float dt);
    void crackAt(Body& b, const vec3& cornerL);
    void leak(Body& b, float dt);

    // ---- Particles: the cubes' water (jets, runs on the sand), splashes, spray, bubbles, plumes, sand.
    enum { P_JET = 0, P_RUN = 1, P_SPLASH = 2, P_SPRAY = 3, P_BUBBLE = 4, P_PLUME = 5, P_SAND = 6 };
    struct Particle {
        vec3 p, v;
        float age, life, size, grav, drag, seed;
        int kind;
        vec3 col;
        bool real;          // ages at the real speed, even in slow motion
        float water = 0.f;  // lines of sea it carries (added when it reaches the sea)
    };
    std::vector<Particle> parts_;
    void splash(const vec3& p, float power, const vec3& col);
    void sprayBurst(float x, float z, int n, float power);
    void sprayAt(float& x, float& z);
    void simulateParticles(float dt, float wdt);

    // ---- The sea: its level (rising with the cleared lines), its swell.
    float waterLines_ = 0.f;  // lines of water poured into the sea this song
    float rise_ = 0.f;        // 0..1 shown (approaches the target)
    float riseTarget() const { return std::min(1.f, waterLines_ / sea::SEA_FULL_LINES); }
    float seaLevel() const { return lerpf(sea::SEA_LOW, sea::SEA_HIGH, rise_); }
    float waveH(float x, float z) const;       // the swell above the sea's level (WORLD_GLSL's waveH)
    float surfY(float x, float z) const { return seaLevel() + waveH(x, z); }
    float amp_ = 0.06f, sharp_ = 0.2f, caps_ = 0.f, glint_ = 0.5f, rock_ = 0.03f;
    float waveT_ = 0.f;
    float dir_ = 1.f;
    float sprayAcc_ = 0.f;

    // ---- Boats: low-poly ships sailing slowly across the sea, bigger as it deepens (the big ones far out).
    struct Boat { float x, z, dir, speed, scale, phase, seed, alpha; int type; };
    std::vector<Boat> boats_;
    float boatT_ = 0.f;
    bool spawnBoat(bool onScreen);

    // ---- Life in the sea, in layers: schools of small fish near the surface, jellyfish drifting in the middle
    // water, and in the dark depths an anglerfish and its lure, siphonophores, points of light; once the sea is very
    // deep, now and then a submarine crossing. Only where there is water, behind the diorama's cut face.
    enum { L_FISH = 0, L_JELLY = 1, L_ANGLER = 2, L_SIPHON = 3, L_MOTE = 4, L_SUB = 5 };
    struct Critter {
        int kind, school;
        float x, z, u, du, dir, speed, size, phase, seed, alpha, dart, dartY, dyv;
        vec3 col, glow;
    };
    std::vector<Critter> critters_;
    int nextSchool_ = 0;
    float lifeT_ = 0.f, glowUp_ = 0.f, subT_ = 15.f;
    float waterDepth() const;                // the sea's depth at the cut face (board units)
    float critterY(const Critter& c) const;
    void spawnLife(bool onScreen);
    void spawnSub();
    void simulateLife(float dt, float wdt, float world);

    // ---- GL.
    struct Mesh { GLuint vao = 0, vbo = 0, ibo = 0; int count = 0; };
    void buildMeshes();
    void worldUniforms(GLuint prog);
    void drawBoats(bool depthOnly);
    void drawBoxes();
    GLuint progSky_ = 0, progSand_ = 0, progSurf_ = 0, progFront_ = 0, progBoat_ = 0, progDepth_ = 0, progCube_ = 0,
           progLife_ = 0, progPart_ = 0, progCell_ = 0;
    Mesh sand_, surf_, front_;
    GLuint meshVbo_ = 0, meshInstVbo_ = 0, boatVao_ = 0, boxVao_ = 0;
    int boatFirst_[6] = {}, boatCount_[6] = {}, boxFirst_ = 0, boxCount_ = 0;
    GLuint cubeVao_ = 0, cubeVbo_ = 0, lifeVao_ = 0, lifeVbo_ = 0;
};

} // namespace biome
