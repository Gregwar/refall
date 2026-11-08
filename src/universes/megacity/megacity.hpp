#pragma once
// MEGACITY: a hut on a riverbank grows into a city covering the whole planet with the cleared lines (see megacity.cpp).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

// The world: meters, the hut at the origin on the ground, y up, the planet's center at (0, -R, 0). The city grows in
// rings around the hut: its radius (the geodesic distance from the hut to its edge) is what the lines drive. Past a
// village, its lots follow a log-polar grid around the hut (square lots whose size grows with the distance: the
// older the ring, the smaller its houses), the same grid on the CPU (the buildings) and in the ground's shader (the
// streets, the lights of the lots too small to draw as buildings).
namespace mcity {
constexpr float R_EARTH = 6.371e6f;
constexpr float LOT_R0 = 200.f;        // where the log-polar grid starts (inside: the village, laid out by hand)
constexpr int SECTORS = 120;           // lots around a ring
constexpr float LOT_D = 6.2831853f / SECTORS;   // a lot's size in log-polar units (radians, and ln r)
constexpr float CITY_FULL = 2.0e7f;    // the planet covered (pi R)
// The river: its centerline's x (meters) at a z along it (near the hut; it fades out far away).
inline float riverX(float z) {
    const float a = std::fabs(z);
    return 14.f + z * 0.16f * std::sin(std::log(a + 30.f) * 2.6f) + 18.f * std::sin(z * 0.011f) * std::min(1.f, a / 150.f);
}
inline float riverHalfW(float z) {
    const float a = std::fabs(z);
    const float t = std::min(1.f, std::max(0.f, (a - 30000.f) / 30000.f));
    return (7.f + 0.035f * a) * (1.f - t * t * (3.f - 2.f * t));
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
} // namespace mcity

// The city, generated once per game from a seed (city.cpp). Every building carries the city radius at which it rises.
struct MegaCityPlan {
    // 16 floats a building (see BLD_VS): base center (world) + birth radius; its x axis + half width; half depth,
    // height, roof height, taper; kind, seed, light key, base height.
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
    float growthOf(int lines) const;    // lines -> G = ln(the city's radius)
    int lines_ = 0, linesShown_ = -1;
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
    struct SpriteInst { float a[4], b[4], c[4]; };
    std::vector<SpriteInst> sprites_;
    void sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param = 0.f, float param2 = 0.f);

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font light_, medium_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, float alpha, float halo = 0.f);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progSky_ = 0, progBld_ = 0, progGlow_ = 0, progComp_ = 0, progBright_ = 0, progBlur_ = 0, progSprite_ = 0,
           progVeil_ = 0, progCell_ = 0, progShape_ = 0, progText_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, meshVbo_ = 0, bldVao_ = 0, bldVbo_ = 0, glowVao_ = 0, glowVbo_ = 0,
           spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
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
    float tetrisGlow_ = 0.f;
    float traffic_ = 0.f, trafficPhase_ = 0.f;
    float beatClock_ = 0.f;
    double fakeBeat_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};
