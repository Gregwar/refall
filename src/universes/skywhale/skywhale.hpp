#pragma once
// SKY WHALE: a koi that grows with every cleared line into a whale swimming up out of the sea, through the clouds
// and around the Earth (see skywhale.cpp).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class SkyWhaleUniverse : public Universe {
public:
    ~SkyWhaleUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void levelUp() override;
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

    // A variant's colors (linear RGB) and light (floats only: palettes are blended float by float).
    struct Palette {
        vec3 zenith, horizon, space, nebula, nebula2, sun, sunGlow, lit, shadow, haze, rim;
        vec3 groundA, groundB, ocean, land, desert, flower, sand, pad, lotus, water, deep, tree, treeShade;
        vec3 koi, koiPatch, koiPatch2, back, belly, fin, glow, eye, aurora, aurora2, lantern;
        vec3 cloudLit, cloudShade, ink, accent, veil;
        vec3 piece[7];
        float sunAz, sunEl, sunSize, night, stars, patch2, auroraSky, bloom, glowK;
    };

    // Where the creature is: its center, its axes (x forward, y up, z its right side), its length (meters).
    struct Pose { vec3 c, fx, fy, fz; float L, bend; vec3 wo; }; // wo: the wandering's offset (meters)

private:
    // ---- The growth: G = ln(the creature's length in meters), from a 30 cm koi to a million kilometers.
    float growthOf(float lines) const;
    float lines_ = 0, linesShown_ = -1, here_ = 0; // paceLines(): 50 at scene completion
    float gTarget_ = 0.f, g_ = 0.f, gVel_ = 0.f;
    float gBonus_ = 0.f;
    float stroke_ = 0.f;          // a clear's tail stroke (its glow)
    float burst_ = 0.f;           // a Tetris' burst
    float hatchT_ = 99.f;         // a new game: a new koi
    float swimPh_ = 0.f, finPh_ = 0.f, singPh_ = 0.f;
    float breachPh_ = 0.f;        // the breaching cycle at sea (0..1)
    float breachKick_ = 0.f;
    uint64_t koiSeed_ = 1;
    float koiSeedF_ = 0.3f;
    Pose pose_{};
    Pose poseAt(float G) const;          // the path (from the growth and the cruise) and the swim's own wandering
    // ---- The cruise: it always swims on, between the clears too (round the pond, down the river, then on and on
    // around the planet), at a pace from the music, a clear surging it ahead.
    struct Cruise {
        float pondA = 0.f;   // its angle round the pond (radians, decreasing: clockwise seen from above)
        float riverC = 0.f;  // meters ahead of its growth's place down the river (held short of the sea)
        double orbitA = 0.0; // radians around the body it circles (the sea, the sky, orbit)
        float moonA = 0.f;   // radians on along the Moon's meridian, gliding low over it (the finale)
    };
    Cruise cr_{};
    float pace_ = 1.f;           // the cruise's pace from the music (smoothed)
    float speedL_ = 0.3f;        // the cruise's speed along its line now (body lengths a second)
    // ---- The swim, physically: the tail's strokes give thrust, the water's (or the air's) drag slows it, so it
    // accelerates, coasts between strokes; it steers toward slowly moving goals, banking into its turns with a lag.
    float swimV_ = 0.2f;         // its speed (body lengths a second)
    float effort_ = 0.5f;        // the strokes' effort (amplitude), 0 gliding .. ~2 a clear's power strokes
    float tailF_ = 1.f;          // the tail's beat (Hz)
    float boost_ = 0.f;          // power strokes left (a clear)
    bool burstOn_ = true;        // the koi's burst-and-coast
    float yaw_ = 0.f, yawRate_ = 0.f, pitch_ = 0.f, pitchRate_ = 0.f; // its heading off its line (radians, /s)
    float offS_ = 0.f, offU_ = 0.f;  // its offset off its line (body lengths: to the side, up)
    float roll_ = 0.f, curv_ = 0.f, curvV_ = 0.f, finTurn_ = 0.f, pathTurn_ = 0.f;
    float rollEvT_ = 40.f, rollEvPh_ = -1.f; // a slow barrel roll now and then (in the air, in space)
    float steerT_ = 0.f;         // the steering goals' clock
    vec3 hPrev_{0.f};
    void updateSwim(float G, float dt, float target);
    // ---- Seabirds (boids) and hot-air balloons: their own lives in the world, around its journey.
    struct Bird { vec3 p, v; float flap, glide, glideT, seed, age, fade, bank; int flock; bool leaving; };
    std::vector<Bird> birds_;
    std::vector<vec3> flockDir_;
    float flockT_ = 0.f;
    struct Balloon { vec3 p; float size, seed, age; int color; };
    std::vector<Balloon> balloons_;
    void updateBirds(float G, float dt);
    vec3 camH_{1.f, 0.f, 0.f};   // the camera's bearing (the travel's direction, smoothed)
    vec3 camWo_{0.f};            // how much of the wandering the camera follows (smoothed)
    bool camHSet_ = false;
    vec3 pathPosC(float G, const Cruise& c) const; // the path's center at G, at that cruise
    vec3 pathPos(float G) const { return pathPosC(G, cr_); }
    vec3 travelDir(float G) const;      // the cruise's direction at G (unit)
    void advanceCruise(float G, float dt);
    vec3 lastHead_{0.f}, lastTail_{0.f};
    vec3 splashC_{0.f};
    float splashAge_ = 99.f;
    float sink_ = 0.f;

    // ---- The camera (world: meters, the pond at the origin, y up; drawn relative to the eye).
    vec3 eye_{0, 1, 10}, look_{0, 0, -1}, camR_{1, 0, 0}, camU_{0, 1, 0}, camUp_{0, 1, 0};
    float tanH_ = 0.5f, aspect_ = 1.78f;
    mat4 proj3_, vp3_;
    float gc_ = -99.f, gcVel_ = 0.f;
    float alt_ = 1.f;
    vec3 camAnchor_{0.f};
    bool camInit_ = false;
    void updateCamera(float dt);
    vec2 toBoard(const vec3& world) const;

    // ---- Particles in the world: spray, sparkles, petals, streaks, rings of song.
    struct Spark { vec3 p, v; float age, life, size, seed, spin; int kind; vec3 col; };
    std::vector<Spark> sparks_;
    float sparkAcc_ = 0.f, streakAcc_ = 0.f, dustAcc_ = 0.f;
    struct Ring { vec3 c, n; float age, life, r0, r1, hue; };
    std::vector<Ring> rings_;
    double lastRingBeat_ = -1e9;
    float ringClock_ = 0.f;
    std::vector<float> ginst_;
    std::vector<float> ribbon_;
    // ---- Clouds.
    struct Puff { vec3 p; float r, seed, grp; }; // (grp: its cloud's own moment to dissolve on the way up, 0..1)
    std::vector<Puff> clouds_;
    std::vector<float> cinst_;
    float cloudDrift_ = 0.f, seaDrift_ = 0.f, flowT_ = 0.f;
    // ---- The banks: trees and bushes (toon puffs), reeds, grass.
    std::vector<float> trees_;
    struct Tuft { vec3 p; float h, seed; int kind; };
    std::vector<Tuft> tufts_;
    void buildWorld();
    // ---- Props: the lighthouse, stone lanterns (meshes turned on the CPU).
    struct PropMesh { GLuint vao = 0, vbo = 0; GLsizei count = 0; };
    PropMesh lighthouse_, toro_;
    void buildProps();

    // ---- The creature's mesh (a grid per part, placed by the shaders).
    GLuint whaleVao_ = 0, whaleVbo_ = 0, whaleIbo_ = 0;
    GLsizei whaleCount_ = 0;
    void buildWhale();

    // ---- 2D motes over the board (the cleared rows flowing to the creature).
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 3.f, size = 0.1f, bright = 1.f, seed = 0.f;
        vec3 col{1.f};
        int kind = 0;   // 0 drifting, 1 flying along a curve to the creature, 3 a hard drop's bubbles
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
    void cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase, float shine);
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
    Font caps_, ital_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, float alpha, float halo = 0.f);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progSky_ = 0, progWhale_ = 0, progProp_ = 0, progPuff_ = 0, progGlow_ = 0, progRibbon_ = 0, progCloud_ = 0,
           progSea_ = 0, progComp_ = 0, progBright_ = 0, progBlur_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0,
           progShape_ = 0, progText_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, puffVao_ = 0, puffVbo_ = 0, glowVao_ = 0, glowVbo_ = 0, cloudVao_ = 0, cloudVbo_ = 0,
           ribbonVao_ = 0, ribbonVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0,
           shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    ugl::Target skyT_, worldT_, bloomA_, bloomB_, sceneT_;
    // The world's geometry is drawn multisampled at (up to) the full resolution over the ray-traced sky (which stays
    // at a reduced resolution, upsampled with its depth), then resolved into worldT_.
    GLuint skyDepthTex_ = 0, skyFbo_ = 0, msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    int msW_ = 0, msH_ = 0, msSamples_ = 1;
    GLuint progUp_ = 0, progLunar_ = 0;
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
    float dropAmt_ = 0.f, swell_ = 0.f, sing_ = 0.f;
    float kickIn_ = 0.f;
    float levelGlow_ = 0.f;
    float beatClock_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{9917};
};
