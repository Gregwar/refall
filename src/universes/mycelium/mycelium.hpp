#pragma once
// MYCELIUM: a glowing fungal network grows over the forest floor at night with the cleared lines, from a single spore
// among the moss until it wraps the whole Earth in filaments of light (see mycelium.cpp).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

// The world is in meters: the spore at the origin on the planet's surface, y up, the planet's center at (0, -R, 0).
// The network grows in a plane of geodesic polar coordinates around the spore (its distance along the surface, its
// bearing), mapped onto the sphere, so a millimeter of threads and a web around the Earth are the same thing.
// The growth is G = ln(the network's reach in meters): two millimeters at the start, the whole planet at the end.
namespace myc {
constexpr double R_EARTH = 6.371e6;
constexpr float G_MIN = -6.2f;          // a couple of millimeters
constexpr float G_MAX = 16.6f;          // around the globe, all but the antipode's surroundings (16 000 km)
// A point of the plane (x, z: meters along the surface from the spore) on the sphere, `lift` meters above it.
inline vec3 toWorld(double x, double z, double lift = 0.0) {
    const double r = std::sqrt(x * x + z * z);
    if (r < 1e-12) return vec3(0.f, (float)lift, 0.f);
    const double a = r / R_EARTH;
    const double s = std::sin(a), h = 2.0 * std::sin(a * 0.5) * std::sin(a * 0.5);
    const double nx = x / r * s, ny = std::cos(a), nz = z / r * s;
    return vec3((float)(R_EARTH * nx + lift * nx), (float)(-R_EARTH * h + lift * ny), (float)(R_EARTH * nz + lift * nz));
}
inline vec3 normalAt(double x, double z) {
    const double r = std::sqrt(x * x + z * z);
    if (r < 1e-12) return vec3(0.f, 1.f, 0.f);
    const double a = r / R_EARTH;
    return vec3((float)(x / r * std::sin(a)), (float)std::cos(a), (float)(z / r * std::sin(a)));
}
float growthOf(float lines);  // lines (on the 50-line scale) -> G
} // namespace myc

// The network, grown once per game from a seed (network.cpp). Every thread carries the growth at which it is born,
// and the shaders reveal it against the growth uniform: nothing is rebuilt while it grows.
struct MycNet {
    // Threads: 16 floats each: A (world) + width | B (world) + kind (0 cord, 1 hypha, 2 hair, 3 fusion, 4 knot) |
    // birth g0, g1, pulse coordinate q0, q1 | lineage hash, thickness class, ln(distance), 0.
    std::vector<float> seg;
    // Places along the threads where mushrooms can come up: the plane's x, z, the growth that reaches it, q.
    struct Site { float x, z, g, q; };
    std::vector<Site> sites;
};
MycNet buildMycNet(uint64_t seed);

class MyceliumUniverse : public Universe {
public:
    ~MyceliumUniverse() override;
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

    // A variant's colors (linear RGB), floats only (palettes are blended float by float).
    struct Palette {
        vec3 skyTop, skyLow, dusk, mist, spaceC, nebula, moonC, ambient;
        vec3 moss, soil, litter, field, forest, land, ocean, city;
        vec3 flora, floraLit, bark, rock;
        vec3 thread, threadTip, pulse, cap[3], stem, cloudLit, cloudShade;
        vec3 ink, accent, veil;
        vec3 piece[7];
        float glow, pool, stars, bloom;
    };

private:
    // ---- The network: its growth G = ln(reach in meters).
    MycNet net_;
    uint64_t netSeed_ = 0;
    bool netDirty_ = true;
    void uploadNet();
    float lines_ = 0, linesShown_ = -1; // paceLines(): 50 at scene completion
    float gTarget_ = 0.f, g_ = 0.f, gVel_ = 0.f, gBonus_ = 0.f;
    float spurt_ = 0.f, burst_ = 0.f;
    float newT_ = 99.f;   // a new game: a new spore

    // ---- Mushrooms: planned once per game from the network's sites, each bigger than the last.
    struct Mushroom {
        float x, z;          // on the plane (meters)
        float g, h0, capR;   // born at growth g, its height then (m), its cap's radius (in heights)
        float seed, lean, leanA;
        int type, color;
        float flare = 0.f, flareT = 99.f;
        bool burst = false;
    };
    std::vector<Mushroom> shrooms_;
    void planMushrooms();
    float shroomHeight(const Mushroom& m, float G) const;
    float shroomOpen(const Mushroom& m, float G) const;
    std::vector<float> minst_;

    // ---- Ground cover at every scale (moss, grass, ferns, bushes, trees), placed once.
    struct Plant { float x, z, s, seed; int level, kind; };
    std::vector<Plant> plants_;
    float macro_ = 1.f;          // 1 at the macro start (the first centimeter), 0 from some 30 cm on
    void buildFlora();
    std::vector<float> finst_;   // puffs (sphere billboards)
    // ---- Houses of a village, for scale.
    struct House { float x, z, w, h, seed; int kind; };
    std::vector<House> houses_;
    vec3 villageW_{0.f};

    // ---- The camera (meters; drawn relative to the eye).
    vec3 eye_{0, 1, 10}, look_{0, 0, -1}, camR_{1, 0, 0}, camU_{0, 1, 0};
    float tanH_ = 0.5f, aspect_ = 1.78f;
    mat4 proj3_, vp3_;
    float gc_ = -99.f, gcVel_ = 0.f;   // the growth the camera frames (it lags behind a spurt)
    float pull_ = 0.f;                 // around a song (setup / teardown): the camera framing a little more (log reach)
    float alt_ = 1.f, camD_ = 1.f;
    vec3 moonDir_{0, 0.2f, -1}, sunDir_{0, -1, 0};
    void updateCamera(float dt);
    vec2 toBoard(const vec3& world) const;

    // ---- Pulses of light out along the threads (q: the pulse coordinate, ~ ln of the distance along the threads).
    struct Pulse { float q, speed, amp, frac, width; };
    std::vector<Pulse> pulses_;
    float waveQ_ = -99.f, waveSpeed_ = 1.f, waveAmp_ = 0.f;   // the drop's wave
    double lastBeat_ = -1.0;
    int beatCount_ = 0;
    float beatClock_ = 0.f;

    // ---- Particles in the world: spores, sparks.
    struct Spore { vec3 p, v; float age, life, size, seed; vec3 col; int kind; };
    std::vector<Spore> spores_;
    float sporeAcc_ = 0.f, spaceAcc_ = 0.f;
    std::vector<float> ginst_;
    // ---- Clouds: toon cumulus in a layer.
    struct Puff { vec3 p; float r, seed; };
    std::vector<Puff> clouds_;
    std::vector<float> cloudA_;  // each puff's opacity, eased toward its wanted one at a limited rate (-1: unset)
    float cloudDrift_ = 0.f, seaDrift_ = 0.f;
    bool cloudsOn_ = false;
    float seaA_ = -1.f;          // the sea of clouds' opacity, eased like the puffs'
    vec3 prevEye_, eyeVel_;      // the camera's smoothed velocity (meters a second)
    bool eyeSet_ = false;
    std::vector<float> cinst_;
    void buildClouds();

    // ---- 2D motes over the board (the cleared rows flying to the network's heart).
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 1.f, size = 0.1f, bright = 1.f, seed = 0.f;
        vec3 col{1.f};
        int kind = 0;   // 1 flying along a curve, 3 a hard drop's puff
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
    GLuint progSky_ = 0, progThread_ = 0, progShroom_ = 0, progPuff_ = 0, progGlow_ = 0, progCloud_ = 0, progSea_ = 0,
           progComp_ = 0, progBright_ = 0, progBlur_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0,
           progPost_ = 0;
    GLuint quadVbo_ = 0, threadVao_ = 0, threadVbo_ = 0, shroomVao_ = 0, shroomVbo_ = 0, shroomIbo_ = 0, shroomInst_ = 0,
           puffVao_ = 0, puffVbo_ = 0, glowVao_ = 0, glowVbo_ = 0, cloudVao_ = 0, cloudVbo_ = 0, spriteVao_ = 0,
           spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    GLsizei threadCount_ = 0, shroomIdx_ = 0;
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
    float kickIn_ = 0.f, kickGlow_ = 0.f;
    float quadGlow_ = 0.f, heart_ = 0.f;
    float windPhase_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{4471};
};
