#pragma once
// ORBITAL STATION: a space station building itself in orbit, one module docked per cleared line (see orbital.cpp).
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

namespace orbital {
constexpr float R_PLANET = 6.371e6f;  // the planet's radius (meters)
constexpr float ALT = 4.2e5f;         // the station's orbit
// The habitat wheel (ring 1) and the great forest ring (ring 2), in the station's frame (meters, y up).
const vec3 WHEEL_C(0.f, 0.f, -50.f);
const vec3 WHEEL_N(0.f, 0.f, 1.f);
constexpr float WHEEL_R = 48.f, WHEEL_TUBE = 4.2f;
const vec3 GREAT_C(0.f, -260.f, -22.2f);
const vec3 GREAT_N(0.f, 1.f, 0.f);
constexpr float GREAT_R = 9000.f, GREAT_HW = 1250.f, GREAT_WALL = 300.f;
// The planet's center (the station orbits 420 km over it), the Moon (moved in close, for the show).
const vec3 EARTH_C(0.f, -(R_PLANET + ALT), 0.f);
constexpr float R_MOON = 1.737e6f;
const vec3 MOON_C(-5.27e7f, -(R_PLANET + ALT) + 1.27e7f, 0.91e7f);

// A frame: an origin and three axes (orthonormal), mapping local points to the station's frame.
struct Xf {
    vec3 o{0.f}, x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
    vec3 at(const vec3& p) const { return o + x * p.x + y * p.y + z * p.z; }
    vec3 dir(const vec3& v) const { return x * v.x + y * v.y + z * v.z; }
};
vec3 rotateAxis(const vec3& v, const vec3& axis, float ang);
} // namespace orbital

class OrbitalUniverse : public Universe {
public:
    ~OrbitalUniverse() override;
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

    // A solid: center + shape, axis X + material, axis Y + p0, axis Z + p1, color + seed, (p2, p3, glow, -).
    struct Part { float a[4], x[4], y[4], z[4], c[4], e[4]; };
    // A light: position + radius (meters), color + intensity, direction + mode (0 lamp, 2 plume, 3 glint).
    struct Glow { float p[4], c[4], d[4]; };

    // A variant (linear RGB; floats only: palettes blend float by float).
    struct Palette {
        vec3 space, milky, sunC, sunGlow, keyC, earthC, amb, envUp, envDown, sunDir, moonDir, keyDir, pole;
        vec3 ocean, shallow, land, land2, desert, ice, cloud, city, atmoDay, atmoTwi, airglow, aurora1, aurora2;
        vec3 hull, hull2, gold, solar, radiator, dark, window, trim, navR, navG, lamp, chase, thrust;
        vec3 landA, landB, lake, rock, town, ringCloud;
        vec3 ink, accent, veil, rail;
        vec3 piece[7];
        float keyMoon, planetType, clouds, cityK, auroraK, milkyK, moonSize, sunVis, sunSize, bloom, nightLift, stars, keyK, ringDay, haze;
    };

    // The station's plan: one module per cleared line (station.cpp).
    enum Kind {
        K_CAPSULE, K_NODE, K_LAB, K_WINGS, K_CUPOLA, K_TRUSS, K_ARRAY, K_RADIATOR, K_SHIP, K_HUB, K_SPOKE, K_ARC,
        K_DISH, K_AXLE, K_BIGSPOKE, K_BIGARC, K_MIRROR, K_LINER, K_NRING, K_YARD, K_TETHER, K_ERING, K_ELEVATOR, K_FLEET,
        K_MOONWORK, K_COLOSSUS, K_COUNT
    };
    struct Module {
        int kind = K_NODE;
        orbital::Xf f;            // its seat (station frame)
        vec3 dim{1.f};            // kind-specific sizes
        float a0 = 0.f, al = 0.f; // arcs (rings), wing angle...
        int ring = 0;             // 0 fixed, 1 turns with the wheel, 2 with the great ring
        vec3 approach{0, 0, -1};  // the side it comes in from (unit, outward from its seat)
        float extent = 5.f;       // its radius (meters)
        vec3 pivot{0.f};          // its middle (local)
        vec4 sph[3];              // its bounds for the camera: spheres (center, radius) on its seat
        int nsph = 1;
        float seed = 0.f;
        bool armOk = false;       // within the robot arm's reach
        int tugs = 1;
        float order = 0.f;        // its lights' place in the drop's sequence (0 the core .. 1 the rim)
        // live state
        int state = 0;            // 0 not built, 1 coming in, 2 docked
        float t = 0.f;            // its flight's time (s)
        bool armed = false;       // the arm brings it in
        vec3 p0{0.f};             // where its flight starts
        float roll0 = 0.f;
        float present = 1.f;      // a new game takes it away (shrinking)
        float latch = 0.f;        // the docking's light
        bool fading = false;      // a new game takes it away
        float fadeT = 0.f;        // since it started fading (negative: waiting its turn)
    };

private:
    // ---- The plan and its framing.
    std::vector<Module> plan_;
    std::vector<vec3> frameA_;    // the bounds' center once module i is in
    void buildPlan();
    int wheelFirst_ = 0, wheelLast_ = 0, greatFirst_ = 0, greatLast_ = 0;
    float lines_ = 0, linesShown_ = -1; // paceLines(): 50 at scene completion
    int built_ = 1;               // modules wanted (lines + the capsule)
    float wheelSpin_ = 0.f, wheelW_ = 0.f, greatSpin_ = 0.f, greatW_ = 0.f;
    // Every ring: its center and axis, the modules that make it (complete: it turns), its turn and speed.
    struct Ring {
        vec3 c{0.f}, n{0, 1, 0}, ref{1, 0, 0};
        int first = -1, last = -1;
        float spin = 0.f, w = 0.f, baseW = 0.f, peakW = 0.f, dropW = 0.f;
        float heads = 4.f, turn = 16.f;
    };
    static constexpr int RINGS = 12;
    Ring rings_[RINGS];
    int stageFirst_[16] = {};   // the first module of each later stage (see buildPlan)
    float moonWork_ = 0.f;      // the Moon's works, 0..1
    int wantOf(float lines) const; // lines on the 50-line scale
    float sparkAcc_ = 0.f;
    void startFlight(int i, float delay);
    float flightA(const Module& m) const;
    float flightB(const Module& m) const;
    orbital::Xf moduleXf(const Module& m, float* tugAccel, vec3* tugPos, bool* tugOn) const;
    orbital::Xf ringXf(int ring) const;

    // ---- Building the frame's solids and lights.
    struct Builder;
    void emitModule(Builder& b, const Module& m, int idx);
    void emitTug(Builder& b, const vec3& pos, const vec3& face, float size, const vec3& exhaust, float thrust, float k, float seed);
    void emitArm(Builder& b, float dt);
    void emitDrones(Builder& b);
    void emitMoon(Builder& b);
    void emitTraffic(Builder& b);
    std::vector<Part> boxes_, fine_, coarse_;
    std::vector<Glow> glows_;
    float lampK(int group, float order) const;
    float chaseAt(float ang, int ring) const;

    // ---- The robot arm (on the first lab): its tip eased between rest and the module it brings in.
    vec3 armTip_{0.f};
    bool armInit_ = false;

    // ---- Sparks (welding at the seats), in the station's frame.
    struct Spark { vec3 p, v; float age, life, size; vec3 col; };
    std::vector<Spark> sparks_;

    // ---- Camera (station frame, meters; drawn relative to the eye).
    vec3 eye_{0, 0, 30}, look_{0, 0, -1}, camR_{1, 0, 0}, camU_{0, 1, 0};
    float tanH_ = 0.47f, aspect_ = 1.78f;
    mat4 vp3_;
    float camF_ = -1.f, camFv_ = 0.f;  // framing radius (log space, a spring)
    vec3 camA_{0.f}, camAv_{0.f};
    float camAlt_ = orbital::ALT;
    void updateCamera(float dt);
    vec2 toBoard(const vec3& w) const;
    float minPx_ = 0.001f;

    // ---- 2D motes over the board (a cleared row's light flying out to the station).
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 1.f, size = 0.1f, bright = 1.f;
        vec3 col{1.f};
        int kind = 0;   // 0 drifting, 1 along a curve
        vec2 from{0, 0}, ctl{0, 0}, to{0, 0};
    };
    std::vector<Mote> motes_;

    // ---- Pieces.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase, float glow);
    void buildBoard(const Game& g);
    struct Fading { float x, y; int type, mask; float t; };
    std::vector<Fading> fading_, overCells_;
    struct SpriteInst { float a[4], b[4], c[4]; };
    std::vector<SpriteInst> sprites_;
    void sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param = 0.f, float param2 = 0.f);

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudRect>& panels);
    float fade_ = 1.f; // the frame's fade in (the post's, the lettering's too)
    text::Lettering lettering_[2]; // its halo: the usual one, the celebrations' and the dialogs' fuller one

    // ---- GL.
    GLuint progSky_ = 0, progBg_ = 0, progBox_ = 0, progParam_ = 0, progBoxSh_ = 0, progParamSh_ = 0, progGlow_ = 0,
           progBright_ = 0, progBlur_ = 0, progComp_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0,
           progPost_ = 0;
    GLuint quadVbo_ = 0, cubeVbo_ = 0, fineVbo_ = 0, fineIbo_ = 0, coarseVbo_ = 0, coarseIbo_ = 0;
    GLuint boxVao_ = 0, boxInst_ = 0, fineVao_ = 0, fineInst_ = 0, coarseVao_ = 0, coarseInst_ = 0;
    GLsizei fineCount_ = 0, coarseCount_ = 0;
    GLuint glowVao_ = 0, glowVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0,
           shapeVbo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0, shadowFbo_ = 0, shadowTex_ = 0;
    int msSamples_ = 4;
    static constexpr int SHADOW_N = 2048;
    ugl::Target skyT_, resolveT_, bloomA_, bloomB_, bloomC_, bloomD_, sceneT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    float pxPerUnit_ = 20.f;
    void drawParts(GLuint progBox, GLuint progParam, bool shadow);
    void uploadParts();

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
    float dropAmt_ = 0.f, dropSeq_ = 0.f, swell_ = 0.f, halo_ = 0.f;
    float groupOn_[3] = {1.f, 0.f, 0.f};
    float kickIn_ = 0.f, kickGlow_ = 0.f;
    float quadT_ = 99.f;
    double beat_ = 0.0;
    int lastHalf_ = -1;
    float planetSpin_ = 0.f, cloudSpin_ = 0.f;
    float wm_ = 0.f, wp_ = 0.f, life_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    // The scene's setup and teardown: the station powered up from the core outward (0 .. 1) and down from the rim
    // inward (0 .. 1), the camera drawn back around them (0 .. 1).
    float powerUp_ = 1.f, powerDown_ = 0.f, away_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x0B17};
};
