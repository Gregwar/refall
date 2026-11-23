#pragma once
// ISLANDS: one continuous flight over an endless tropical archipelago, like drone footage, behind the well, in
// flattened low poly; the pieces are low poly cubes (see islands.cpp).
#include <memory>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"
#include "universes/islands/world.hpp"

class IslandsUniverse : public Universe {
public:
    ~IslandsUniverse() override;
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

private:
    using Inst = isl::Inst;
    // ---- The world.
    std::unique_ptr<isl::World> world_;
    int builtVariant_ = -1;
    void buildWorld();
    void uploadMeshes();

    // ---- The flight: the camera rides the route, its altitude, speed, heading and bank on critically damped
    // springs (smooth to the second order), from the music and what is ahead.
    struct Spring {
        float x = 0.f, v = 0.f;
        void step(float target, float omega, float dt);
    };
    double s_ = 0.0;                       // along the route
    Spring logAlt_, speed_, yawOff_, pitch_, bank_;
    float alt_ = 60.f, yaw_ = 0.f, vel_ = 20.f;
    float calmPhase_ = 0.f, cruisePhase_ = 0.f, diveT_ = 99.f, buildK_ = 0.f, heroW_ = 0.f, offTgt_ = 0.f;
    bool camInit_ = false;
    vec3 eye_{0, 60, 0}, fwd_{1, 0, 0}, up_{0, 1, 0}, right_{0, 0, 1};
    float curv_ = 0.f, near_ = 0.5f, far_ = 40000.f, fov_ = 50.f;
    void fly(float dt, float wdt, const std::string& section);
    float floorAhead(const vec3& from, float yaw, float v, float& hard) const;
    float tgtPrev_ = 0.f, tgtVel_ = 0.f;   // the floor ahead (log) last frame, its rate
    Spring hardS_;                         // the least altitude now (see floorAhead), eased in
    float clear_ = 60.f;                   // the eye's height over the ground (and what stands on it) under it

    mat4 view_, proj_, vp_, invVp_, lightVp_;
    mat4 bvp_;                             // the board's (the layout's)
    vec3 beye_;                            // its eye (board space)
    int cubeCount_ = 0;                    // the pieces' cube mesh (vertices)

    // ---- Life: boats, the seaplane, the birds, the clouds, the lanterns.
    struct Boat {
        int kind = 0;                      // 0 sailboat, 1 catamaran, 2 speedboat, 3 kayak
        vec3 p{0, 0, 0};
        float head = 0, speed = 0, cruise = 0, turn = 0, phase = 0, len = 10, beam = 3, age = 0;
        vec3 col{1, 1, 1};
        int stripe = 0;
        bool moored = false;
        // the fleet (the boats the cleared rows launched): its place along the route, its sails
        float scale = 1.f, unfurl = 1.f, sailCode = 0.f, latN = 0.f, latOff = 0.f, appear = 1.f, sink = 0.f, launchT = 99.f;
        vec3 sailCol{1, 1, 1}, jibCol{1, 1, 1}, launchP{0, 0, 0}, prevP{0, 0, 0};
        float prevS = -1.f;                // (the pop check, REFALL_ISLANDS_POPCHECK)
        double sb = 0.0;
        bool big = false, place = false;   // (place: put it at its place in the fleet, unseen)
        // the helm: its size as drawn (fully grown), the course it wants and the one it steers, how much of the
        // water ahead is open on that one, the open run on its present heading, the next look around
        float size = 1.f, goal = 0.f, want = 0.f, open = 1.f, ahead = 1e9f, plan = 0.f;
        float check = 0.f;                 // the next look at the water under it (and at its place in the fleet)
    };
    std::vector<Boat> boats_;
    std::vector<Boat> fleet_;
    float spread_ = 1.f;
    void launchBoat(const std::vector<int>& types, bool big);
    void updateFleet(float dt);
    void popCheck(std::vector<Boat>& list, const char* what, float dt);
    vec3 seaPointInBand(int side, float dist) const;
    // ---- The drop: the regatta (spinnakers, the fleet racing), golden light, dolphins, fireworks at dusk.
    float dropK_ = 0.f;
    struct Dolphin { vec3 off; float t0 = -9.f, dur = 1.f, size = 2.4f, seed = 0.f; };
    struct Pod { float relF = 60.f, relS = 30.f; std::vector<Dolphin> d; };
    std::vector<Pod> pods_;
    void dropBeat(double beatLen);
    struct Spark { vec3 p, v, col; float t, life; };
    std::vector<Spark> sparks_;
    void firework();
    void releaseFlockAt(const vec3& c, int n);
    void updateBoats(float dt);
    void spawnBoat(bool initial);
    // keeping to open water (the land and what stands in it): a hull's footprint, a course's free run, the helm
    bool seaClear(float x, float z, float r, float depth) const;
    bool aground(const Boat& b) const;
    float seaRun(const Boat& b, float head, float look, float keep) const;
    std::vector<vec3> track_;              // the camera's track ahead (the fleet keeps off it)
    float trackDist(const vec3& q, vec3* nearest = nullptr) const;
    void steer(Boat& b, float goal, float cruise, float keep, float dt);
    void boatParts(const Boat& b);
    float boatTop(const Boat& b, float* reach = nullptr) const;
    struct Plane { bool on = false; vec3 p{0, 0, 0}, v{0, 0, 0}; float bank = 0, t = 0; } plane_;
    float planeWait_ = 15.f;
    void updatePlane(float dt);
    struct Bird { vec3 off{0, 0, 0}; float phase = 0, glide = 0, size = 1; };
    struct Flock {
        vec3 c{0, 0, 0}, v{0, 0, 0};
        float age = 0, life = 10, scale = 1;
        bool frigate = false, follow = false;
        std::vector<Bird> birds;
    };
    std::vector<Flock> flocks_;
    int flockSide_ = 1;
    void releaseFlock(int n, bool big);
    void updateBirds(float dt);
    struct Lantern { vec3 p; float t; float seed; };
    std::vector<Lantern> lanterns_;
    void lightLanterns(int n);
    struct Puff { vec3 rel; float r, seed, alpha, base, shade, d2; };
    std::vector<Puff> puffs_;
    std::vector<float> cloudShadows_;
    void buildClouds(float worldTime);

    // ---- Per-frame geometry.
    std::vector<Inst> inst_[isl::M_COUNT];
    std::vector<float> glows_;             // x y z (relative), size, r g b, soft
    void glow(const vec3& world, float size, const vec3& col, float soft = 0.3f);
    void push(const isl::Part& p);
    void gatherIslands();
    struct Draw { isl::Island* I; float dist; bool shadow; };
    std::vector<Draw> draws_;

    // ---- The well.
    float seeds_[22][10] = {};
    float pieceSeeds_[4] = {0.17f, 0.3f, 0.43f, 0.56f}; // the falling piece's cells' tiles, kept when it locks
    int ids_[22][10] = {};      // each cell's piece (its lock's number * 8 + its type + 1; 0 empty, < 0 alone): its tile
                                // runs on into its piece's other cells
    int lockCount_ = 0;
    float gridMask(int x, int y) const;
    void syncCells(const Game& g);
    void buildBoard(const Game& g);
    struct GlassInst { float a[4], b[4], c[4]; };
    std::vector<GlassInst> glass_;
    struct ClearCell { int x, y, type; float seed, t, delay, mask; };
    std::vector<ClearCell> clearing_;
    struct Grain { vec3 p, v; float t, life, size; };
    std::vector<Grain> grains_;
    std::vector<float> flat_;
    std::vector<float> tiles_;             // FLAT's tiles (flat_'s format, drawn over it)
    void quad(const vec3& c, float hw, float hh, const vec3& col, float a, float sx, float sy);
    void line(float x0, float y0, float x1, float y1, float z, float w, const vec3& col, float a);
    vec3 pieceCol(int type) const;
    float overT_ = -1.f;
    bool hardPending_ = false;

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const Game& g, const std::vector<HudRect>& panels);
    void hudInks(vec3& ink, vec3& halo, vec3& accent) const;
    std::vector<float> shapes_;
    void flushShapes();
    const char* hourName() const;

    // ---- GL.
    GLuint progSky_ = 0, progTerrain_ = 0, progObj_ = 0, progWater_ = 0, progCloud_ = 0, progGlow_ = 0, progFlat_ = 0,
           progGlass_ = 0, progFinal_ = 0, progShape_ = 0, progTrip_ = 0;
    GLuint meshVao_[isl::M_COUNT] = {}, meshVbo_[isl::M_COUNT] = {}, instVbo_[isl::M_COUNT] = {};
    int meshCount_[isl::M_COUNT] = {};
    GLuint idx_[isl::LOD_COUNT] = {};
    int idxCount_[isl::LOD_COUNT] = {};
    GLuint quadVbo_ = 0, glowVao_ = 0, glowVbo_ = 0, cloudVao_ = 0, cloudVbo_ = 0, glassVao_ = 0, glassVbo_ = 0, cubeVbo_ = 0, flatVao_ = 0,
           flatVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    GLuint shadowFbo_ = 0, shadowTex_ = 0;
    // a 1x1 shadow map for the shadow pass's own draws: their uShadow must name a shadow texture that isn't the one
    // being drawn (WebGL refuses a sampler left on another kind of texture, or on the target)
    GLuint shadowDummy_ = 0;
    ugl::Target sceneT_, seabedT_;
    GLuint halfDepth_ = 0;
    int samples_ = 4, halfW_ = 1, halfH_ = 1;
    bool seabedFull_ = false;              // the seabed at full resolution (FLAT)
    void makeSeabed(bool full);
    static constexpr int SHADOW_SIZE = 4096;
    void drawTerrain(GLuint prog, int mode, bool shadowPass);
    void drawObjects(GLuint prog);
    void setCommon(GLuint p);

    // ---- Song, variant, the hour.
    int variant_ = 0;
    uint64_t seed_ = 1;
    float songLen_ = 200.f, songT_ = 0.f, tod_ = 0.f, todShown_ = 0.f;
    bool todInit_ = false;
    struct Sky { vec3 sunDir, sunCol, top, hor, low, fog; float haze; };
    Sky sky_;
    float lights_ = 0.f, night_ = 0.f;
    vec3 backdrop() const;
    // FLAT: the vector illustration (flat fills in two tones, flat pastel tiles, navy lettering)
    bool flat() const;
    vec3 flatLit_{1.f, 1.f, 1.f}, flatShade_{0.78f, 0.82f, 0.9f};
    // ---- TRIP: the swirls' clock (faster with the energy), the soft beat swell, the ringed sun's heading
    bool trip() const;
    float tripScale() const;
    void tripCornerPanes(const Frame& f);
    vec2 tripO_{0.f, 0.f};
    float flowT_ = 0.f, pulse_ = 0.f, tripSunYaw_ = 0.f;
    bool tripSunInit_ = false;
    vec3 tripSun_{1, 0.1f, 0};
    // ---- FLAT's drops: TRIP's neons washing over the same shapes (psy_: the front, 0 natural .. 1 all neon; its
    // state held a while against flip flops)
    float psyT_ = 0.f, psy_ = 0.f, psyHold_ = 100.f, psyOff_ = 0.f;
    bool psyOn_ = false;
    void updatePsy(bool inDrop, float dt);

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float swell_ = 0.f, kickIn_ = 0.f, sparkle_ = 0.f;
    double lastHalf_ = -1.0;
    float time_ = 0.f, worldTime_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f, dimT_ = 0.f;
    bool bare_ = false;
    Bridge bridge_;
    Rng rng_{91};
};
