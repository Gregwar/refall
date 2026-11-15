#pragma once
// MEGACITY: a hut on a riverbank grows into a city covering the whole planet with the cleared lines (see megacity.cpp).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

// The world: meters, the hut at the origin on the ground, y up, the planet's center at (0, -R, 0). The lines drive
// the city's frontier F (meters); each place rises when F passes its organic distance D (see field()): the distance
// from the hut stretched by the terrain (short along the river, the highways and the coast, long across the land),
// warped by noise into lobes and fingers, and short again around satellite towns along the highways, which grow on
// their own and merge. The same field on the CPU (the buildings) and in the ground's shader (the streets, the lights).
namespace mcity {
constexpr float R_EARTH = 6.371e6f;
constexpr float TAU_F = 6.2831853f;
constexpr int ROADS = 6;
constexpr int SATS = 8;
// The river: its centerline's x (meters) at a z along it.
inline float riverX(float z) {
    const float a = std::fabs(z);
    return 14.f + z * 0.16f * std::sin(std::log(a + 30.f) * 2.6f) + 18.f * std::sin(z * 0.011f) * std::min(1.f, a / 150.f);
}
inline float riverHalfW(float z) {
    const float a = std::fabs(z);
    const float t = std::min(1.f, std::max(0.f, (a - 50000.f) / 30000.f));
    return (7.f + 0.022f * a) * (1.f - t * t * (3.f - 2.f * t));
}
// A hash shared with the shaders (no sin: the same result on the CPU and the GPU).
inline float hashLot(float x, float y) {
    float p3x = x * 0.1031f, p3y = y * 0.1031f, p3z = x * 0.1031f;
    p3x -= std::floor(p3x); p3y -= std::floor(p3y); p3z -= std::floor(p3z);
    const float d = p3x * (p3y + 33.33f) + p3y * (p3z + 33.33f) + p3z * (p3x + 33.33f);
    p3x += d; p3y += d; p3z += d;
    const float r = (p3x + p3y) * p3z;
    return r - std::floor(r);
}
inline float vnL(float x, float y) {
    const float ix = std::floor(x), iy = std::floor(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3.f - 2.f * fx); fy = fy * fy * (3.f - 2.f * fy);
    const float a = hashLot(ix, iy), b = hashLot(ix + 1.f, iy), c = hashLot(ix, iy + 1.f), d = hashLot(ix + 1.f, iy + 1.f);
    const float ab = a + (b - a) * fx, cd = c + (d - c) * fx;
    return ab + (cd - ab) * fy;
}
inline float ofbm(float x, float y) {
    return 0.5f * vnL(x, y) + 0.3f * vnL(x * 2.1f + 5.3f, y * 2.1f + 1.1f) + 0.2f * vnL(x * 4.3f + 1.7f, y * 4.3f + 9.2f);
}
// The highways leaving the hut: their bearing at a distance r (winding gently in log space).
inline float roadAngle(int k, float r) {
    return k * TAU_F / ROADS + 0.5f * (hashLot((float)k, 3.f) - 0.5f) + 0.22f * std::sin(std::log(r + 600.f) * 2.1f + 1.7f * k);
}
// The coast of a bay east of the hut: > 0 at sea.
inline float coastSigned(float x, float z) {
    const float along = x * 0.96f + z * 0.28f, across = -x * 0.28f + z * 0.96f;
    return along - (16000.f + 0.42f * std::fabs(across) + 9000.f * (ofbm(x / 11000.f, z / 11000.f) - 0.5f) * 2.f);
}
inline bool atSea(float x, float z) { return x * x + z * z < 2.5e11f && coastSigned(x, z) > 0.f; }
// Satellite towns along the highways: position, the frontier at which each is founded.
struct Sat { float x, z, a; };
inline void satellites(Sat* s) {
    for (int j = 0; j < SATS; j++) {
        const int k = (j * 5 + 1) % ROADS;
        float r = 2600.f * std::pow(1.72f, (float)j);
        float ph = roadAngle(k, r);
        for (int t = 0; t < 8 && atSea(r * std::cos(ph), r * std::sin(ph)); t++) { r *= 0.8f; ph = roadAngle(k, r); }
        s[j] = {r * std::cos(ph), r * std::sin(ph), 0.5f * r};
    }
}
// The distance to the nearest corridor the city follows: the river, the highways, the coast.
inline float corridor(float x, float z, float r) {
    float d = std::fabs(z) < 80000.f ? std::max(0.f, std::fabs(x - riverX(z)) - riverHalfW(z)) : 1e9f;
    const float ph = std::atan2(z, x);
    for (int k = 0; k < ROADS; k++) {
        float a = ph - roadAngle(k, r);
        a -= TAU_F * std::floor(a / TAU_F + 0.5f);
        d = std::min(d, std::fabs(a) * r);
    }
    if (r < 5.e5f) d = std::min(d, std::fabs(coastSigned(x, z)));
    return d;
}
// The organic distance D: when the frontier F passes it, the place is built.
inline float field(float x, float z, const Sat* s) {
    const float r = std::sqrt(x * x + z * z) + 1e-3f;
    const float c = 0.58f + 0.72f * std::min(1.f, std::max(0.f, corridor(x, z, r) / (0.1f * r + 150.f)));
    const float lr = std::log(r + 50.f);
    const float ux = x / r, uz = z / r;
    const float n1 = ofbm(ux * 2.6f + lr * 1.1f, uz * 2.6f + lr * 0.6f);
    const float n2 = ofbm(ux * 8.f - lr * 2.3f + 17.f, uz * 8.f + lr * 1.7f);
    const float n = std::exp((n1 - 0.5f) * 1.5f + (n2 - 0.5f) * 0.7f);
    float D = r * c * n;
    for (int j = 0; j < SATS; j++) {
        const float dx = x - s[j].x, dz = z - s[j].z;
        D = std::min(D, s[j].a + std::sqrt(dx * dx + dz * dz) * c * n * 1.15f);
    }
    // far away (the continents, the planet) a plainer distance, so that the whole planet gets covered in the end
    const float k = std::min(1.f, std::max(0.f, (lr - 12.6f) / 2.4f));
    return D + (r * (0.8f + 0.3f * n1) - D) * k * k * (3.f - 2.f * k);
}
// The tallest towers when the frontier is at F (the skyscraper stage starts at ~4 km).
inline float towerH(float F) { return 95.f * std::pow(std::max(F, 1000.f) / 4500.f, 1.1f); }
constexpr float SKY_F0 = 4200.f;   // the frontier at which the first towers rise downtown
} // namespace mcity

// The city, generated once per game from a seed (city.cpp). Every building carries the city radius at which it rises.
struct MegaCityPlan {
    // 20 floats a building (see BLD_VS): base center (world) + birth; its x axis + half width; half depth, height,
    // roof height, taper; kind, seed, light key, base height; death (it sinks back when the frontier passes it:
    // the old town making way for the towers), twist, -, -.
    std::vector<float> inst;
    // The instances are sorted by birth radius: births[i] is the i-th one's.
    std::vector<float> births;
    // Red lights on the tops of tall buildings: position (world), birth radius; sorted by birth radius.
    std::vector<float> beacons;
    vec3 elevator{0.f, 0.f, 0.f};   // the space elevator's foot (world), its up
    vec3 elevatorUp{0.f, 1.f, 0.f};
    int fixedFirst = 0;             // instances from here on are drawn whatever their size (the elevator's tower)
    // Trees around the hut (meters): x, z, size, seed (they vanish when the city reaches them).
    std::vector<float> trees;
    // Chimney tops (their smoke) and the bridges' lamps: position (world), birth.
    std::vector<float> chimneys, lamps;
    mcity::Sat sats[mcity::SATS];
    float downtown[2] = {1700.f, -350.f};   // the main downtown's center (x, z)
};
MegaCityPlan buildMegaCity(uint64_t seed);

class MegaCityUniverse : public Universe {
public:
    ~MegaCityUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;

    // A variant's colors (linear RGB) and light (floats only: palettes are blended float by float).
    struct Palette {
        vec3 duskZenith, duskHorizon, nightZenith, nightHorizon, space, nebula, sun, sunGlow, moonC;
        vec3 lit, shadow, haze, nightHaze, cityGlow;
        vec3 grass, grass2, field, land, desert, ocean, river, forest;
        vec3 wallA, wallB, roofA, roofB, stone, glass, arco;
        vec3 winWarm, winCool, winAccent, street, traffic, beacon;
        vec3 ink, accent, veil;
        vec3 piece[7];
        float sunAz, snow, aurora, neon, nightBias;
    };

private:
    // ---- The city: its radius (meters) from the lines.
    MegaCityPlan plan_;
    uint64_t citySeed_ = 0;
    bool cityDirty_ = true;
    void uploadCity();
    float growthOf(float lines) const;    // lines -> G = ln(the city's radius)
    float lines_ = 0, linesShown_ = -1; // paceLines(): 50 at scene completion
    float gTarget_ = 0.f, g_ = 0.f, gVel_ = 0.f;   // the growth shown chases its target
    float gBonus_ = 0.f;                             // the drops' extra growth
    float orbit_ = 0.f;                              // past the planet covered: the orbital ring's construction 0..1
    float spurt_ = 0.f;
    float overDark_ = 0.f;                           // game over: the lights go out from the edge in
    float resetT_ = 99.f;

    // ---- The drop's district: the band of radii it raised, its lights coming on block by block on the beat.
    float dropR0_ = 0.f, dropR1_ = 0.f, dropBeats_ = 0.f;
    double dropBeat0_ = 0.0;
    bool dropLit_ = false;

    // ---- The camera (world: meters; drawn relative to the eye).
    vec3 eye_{0, 2, 10}, look_{0, 0, -1}, camR_{1, 0, 0}, camU_{0, 1, 0};
    float tanH_ = 0.5f, aspect_ = 1.78f;
    mat4 vp3_;
    float gc_ = -99.f, gcVel_ = 0.f;
    float alt_ = 1.f, camD_ = 10.f;
    void updateCamera(float dt);
    vec2 toBoard(const vec3& world) const;

    // ---- Lights in the air: sparks, flying traffic, the clear's light landing.
    struct Spark { vec3 p, v; float age, life, size, seed; int kind; vec3 col; };
    std::vector<Spark> sparks_;
    float flyAcc_ = 0.f, fireflyAcc_ = 0.f, lanternAcc_ = 0.f;
    bool lanternBurst_ = false;
    std::vector<float> ginst_;

    // ---- 2D motes over the board (the cleared rows flowing to the city).
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 3.f, size = 0.1f, bright = 1.f, seed = 0.f;
        vec3 col{1.f};
        int kind = 0;   // 1 flying along a curve to the city, 3 a hard drop's puff
        vec2 from{0, 0}, ctl{0, 0}, to{0, 0};
    };
    std::vector<Mote> motes_;
    void addMote(const Mote& m);

    // ---- Pieces.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase);
    void buildBoard(const Game& g);
    struct Fading { float x, y; int type, mask; float t; };
    std::vector<Fading> fading_, overCells_;
    struct SpriteInst { float a[4], b[4]; };
    std::vector<SpriteInst> sprites_;
    void sprite(vec2 p, float size, const vec3& col, float alpha);

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudRect>& panels);
    float fade_ = 1.f; // the frame's fade in (the post's, the lettering's too)
    text::Lettering lettering_[2]; // its halo: the usual one, the celebrations' and the dialogs' fuller one

    // ---- GL.
    GLuint progSky_ = 0, progBld_ = 0, progGlow_ = 0, progComp_ = 0, progBright_ = 0, progBlur_ = 0, progSprite_ = 0,
           progVeil_ = 0, progCell_ = 0, progShape_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, meshVbo_ = 0, bldVao_ = 0, bldVbo_ = 0, glowVao_ = 0, glowVbo_ = 0,
           spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    GLsizei meshCount_ = 0, bldCount_ = 0;
    ugl::Target worldT_, bloomA_, bloomB_, sceneT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    float pxPerUnit_ = 20.f;

    // ---- Song, variant.
    int variant_ = 0;
    bool palSet_ = false;
    Palette pal_{}, basePal_{};
    uint64_t seed_ = 1;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false, wasDrop_ = false;
    float dropAmt_ = 0.f, swell_ = 0.f;
    float beams_ = 0.f;
    float kickIn_ = 0.f;
    float quadGlow_ = 0.f;
    float traffic_ = 0.f, trafficPhase_ = 0.f;
    float beatClock_ = 0.f;
    double fakeBeat_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float camPull_ = 0.f; // the setup / teardown: the camera drawn back (0: its place)
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};
